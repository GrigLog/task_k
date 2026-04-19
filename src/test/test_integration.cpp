#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <string>
#include <thread>

#include "stats/stats_request.h"

#ifndef MALWARE_SCANNER_SERVER
#define MALWARE_SCANNER_SERVER ""
#endif
#ifndef MALWARE_SCANNER_CLIENT
#define MALWARE_SCANNER_CLIENT ""
#endif

namespace {

bool WaitForTcpAccept(std::uint16_t port, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            return false;
        }
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        if (inet_pton(AF_INET, "127.0.0.1", &a.sin_addr) != 1) {
            close(s);
            return false;
        }
        if (connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0) {
            close(s);
            return true;
        }
        close(s);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::string JoinPath(const std::string& dir, const char* name) {
    if (dir.empty() || dir.back() == '/') {
        return dir + name;
    }
    return dir + "/" + name;
}

void WaitPidNoIntr(pid_t pid, int& status) {
    while(true) {
        const pid_t w = waitpid(pid, &status, 0);
        if (w == pid) {
            return;
        }
        ASSERT_EQ(w, static_cast<pid_t>(-1));
        ASSERT_EQ(errno, EINTR) << "waitpid failed";
    }
}

} // namespace

TEST(Integration, TwoClientsThenStatsJson) {
    if (MALWARE_SCANNER_SERVER[0] == '\0' || MALWARE_SCANNER_CLIENT[0] == '\0') {
        GTEST_SKIP() << "Binary paths not defined";
    }

    char dirTmpl[] = "/tmp/ms_e2e_XXXXXX";
    ASSERT_NE(mkdtemp(dirTmpl), nullptr);
    const std::string dir(dirTmpl);
    const std::string cfgPath = JoinPath(dir, "config.json");
    const std::string fifoReq = JoinPath(dir, "req");
    const std::string fifoResp = JoinPath(dir, "resp");
    const std::string f1 = JoinPath(dir, "a.bin");
    const std::string f2 = JoinPath(dir, "b.bin");

    {
        std::ofstream c(cfgPath);
        ASSERT_TRUE(c);
        c << R"({"virus":["x"],"ransomware":["y"]})";
    }
    ASSERT_EQ(mkfifo(fifoReq.c_str(), 0666), 0);
    ASSERT_EQ(mkfifo(fifoResp.c_str(), 0666), 0);

    {
        std::ofstream a(f1);
        ASSERT_TRUE(a);
        a << "xx";
    }
    {
        std::ofstream b(f2);
        ASSERT_TRUE(b);
        b << "yy";
    }

    const std::uint16_t port = static_cast<std::uint16_t>(31000 + (getpid() % 500));
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port));

    const pid_t srv = fork();
    ASSERT_GE(srv, 0);
    if (srv == 0) {
        execl(MALWARE_SCANNER_SERVER, "malware_scanner_server", cfgPath.c_str(), portStr, fifoReq.c_str(),
            fifoResp.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    ASSERT_TRUE(WaitForTcpAccept(port, 8000)) << "server did not accept";

    auto runClient = [&](const char* path) {
        const pid_t c = fork();
        ASSERT_GE(c, 0);
        if (c == 0) {
            execl(MALWARE_SCANNER_CLIENT, "malware_scanner_client", path, portStr, static_cast<char*>(nullptr));
            _exit(127);
        }
        int st = 0;
        ASSERT_EQ(waitpid(c, &st, 0), c);
        ASSERT_TRUE(WIFEXITED(st));
        EXPECT_EQ(WEXITSTATUS(st), 0);
    };
    runClient(f1.c_str());
    runClient(f2.c_str());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string json;
    ASSERT_TRUE(RequestStats(fifoReq, fifoResp, json));
    const nlohmann::json j = nlohmann::json::parse(json);
    EXPECT_EQ(j["files"], 2);
    EXPECT_EQ(j["patterns"]["virus"], 2);
    EXPECT_EQ(j["patterns"]["ransomware"], 2);

    ASSERT_EQ(kill(srv, SIGINT), 0);
    int st = 0;
    WaitPidNoIntr(srv, st);
    ASSERT_TRUE(WIFEXITED(st));
    EXPECT_EQ(WEXITSTATUS(st), 0);
}

TEST(Integration, SigintExitsCleanly) {
    if (MALWARE_SCANNER_SERVER[0] == '\0') {
        GTEST_SKIP() << "MALWARE_SCANNER_SERVER not defined";
    }

    char dirTmpl[] = "/tmp/ms_sig_XXXXXX";
    ASSERT_NE(mkdtemp(dirTmpl), nullptr);
    const std::string dir(dirTmpl);
    const std::string cfgPath = JoinPath(dir, "config.json");
    const std::string fifoReq = JoinPath(dir, "req");
    const std::string fifoResp = JoinPath(dir, "resp");

    {
        std::ofstream c(cfgPath);
        ASSERT_TRUE(c);
        c << "{}";
    }
    ASSERT_EQ(mkfifo(fifoReq.c_str(), 0666), 0);
    ASSERT_EQ(mkfifo(fifoResp.c_str(), 0666), 0);

    const std::uint16_t port = static_cast<std::uint16_t>(32000 + (getpid() % 500));
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port));

    const pid_t srv = fork();
    ASSERT_GE(srv, 0);
    if (srv == 0) {
        execl(MALWARE_SCANNER_SERVER, "malware_scanner_server", cfgPath.c_str(), portStr, fifoReq.c_str(),
            fifoResp.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    ASSERT_TRUE(WaitForTcpAccept(port, 8000));
    ASSERT_EQ(kill(srv, SIGINT), 0);
    int st = 0;
    WaitPidNoIntr(srv, st);
    ASSERT_TRUE(WIFEXITED(st));
    EXPECT_EQ(WEXITSTATUS(st), 0);
}

TEST(Integration, StatsWriterBlocksUntilServerOpensFifo) {
    if (MALWARE_SCANNER_SERVER[0] == '\0') {
        GTEST_SKIP() << "MALWARE_SCANNER_SERVER not defined";
    }

    char dirTmpl[] = "/tmp/ms_fifo_XXXXXX";
    ASSERT_NE(mkdtemp(dirTmpl), nullptr);
    const std::string dir(dirTmpl);
    const std::string cfgPath = JoinPath(dir, "config.json");
    const std::string fifoReq = JoinPath(dir, "req");
    const std::string fifoResp = JoinPath(dir, "resp");

    {
        std::ofstream c(cfgPath);
        ASSERT_TRUE(c);
        c << "{}";
    }
    ASSERT_EQ(mkfifo(fifoReq.c_str(), 0666), 0);
    ASSERT_EQ(mkfifo(fifoResp.c_str(), 0666), 0);

    const std::uint16_t port = static_cast<std::uint16_t>(33000 + (getpid() % 500));
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port));

    std::future<int> openFut = std::async(std::launch::async, [&fifoReq]() {
        return open(fifoReq.c_str(), O_WRONLY);
    });

    ASSERT_EQ(openFut.wait_for(std::chrono::milliseconds(500)), std::future_status::timeout);

    const pid_t srv = fork();
    ASSERT_GE(srv, 0);
    if (srv == 0) {
        execl(MALWARE_SCANNER_SERVER, "malware_scanner_server", cfgPath.c_str(), portStr, fifoReq.c_str(),
            fifoResp.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    const int wrFd = openFut.get();
    ASSERT_GE(wrFd, 0);
    close(wrFd);

    ASSERT_EQ(kill(srv, SIGINT), 0);
    int st = 0;
    WaitPidNoIntr(srv, st);
    ASSERT_TRUE(WIFEXITED(st));
    EXPECT_EQ(WEXITSTATUS(st), 0);
}

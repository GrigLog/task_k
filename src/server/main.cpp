#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "common/scanner.h"
#include "server/fd_resource.h"
#include "server/listen_socket.h"
#include "server/stats_store.h"
#include "server/worker_list.h"
#include "server/worker_child.h"


bool ReadChar(int fd, char& ch) {
    const ssize_t n = read(fd, &ch, 1);
    if (n == 0) {
        return false;
    }
    if (n < 0) {
        if (errno == EINTR) {
            return ReadChar(fd, ch);
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        return false;
    }
    return true;
}

void RemoveChildReadFd(std::vector<UniqueFd>& v, int fd) {
    const auto it = std::find_if(v.begin(), v.end(),
        [fd](const UniqueFd& u) { return u.get() == fd; });
    if (it != v.end()) {
        v.erase(it);
    }
}

void ApplyChildJsonLine(TStatsStore& stats, const std::string& line) {
    if (line.empty()) {
        return;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(line);
        if (!j.is_object()) {
            return;
        }
        std::map<std::string, std::uint64_t> counts;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it->is_number_unsigned()) {
                counts[it.key()] = it->get<std::uint64_t>();
            } else if (it->is_number_integer() && it->get<std::int64_t>() >= 0) {
                counts[it.key()] = static_cast<std::uint64_t>(it->get<std::int64_t>());
            }
        }
        stats.AddScanResult(counts);
    } catch (...) {
    }
}

void HandleStatRequests(pollfd& p, TStatsStore& stats, const std::string& fifoResponseName) {
    if (!(p.revents & (POLLIN | POLLHUP)))
        return;
    const int requestFd = p.fd;
    // The stats client writes the request byte, then opens the response fifo for read (blocking until
    // a writer exists). If we block on open(O_WRONLY) here, the whole poll loop stops — no TCP accept.
    // So: try non-blocking open first; if the reader is not ready yet (ENXIO), leave bytes in the
    // request fifo and return; we'll run again after the client opens the response fifo.
    while (true) {
        UniqueFd respOut = UniqueFd::tryOpenFifoWriteNonBlocking(fifoResponseName.c_str());
        if (!respOut) {
            return;
        }
        char ch = 0;
        if (!ReadChar(requestFd, ch)) {
            return;
        }
        const std::string json = stats.ToJson().dump();
        const char* pout = json.data();
        std::size_t left = json.size();
        while (left > 0) {
            const ssize_t nw = write(respOut.get(), pout, left);
            if (nw <= 0)
                break;
            pout += static_cast<std::size_t>(nw);
            left -= static_cast<std::size_t>(nw);
        }
        // respOut closes here => EOF on reader (ReadAll in stats client completes).
    }
}

void HandleSignal(pollfd& p, bool& shutdown, UniqueFd& listenSock, TWorkerList& workers) {
    if (!(p.revents & POLLIN))
        return;
    int signalFd = p.fd;
    while(true) {
        signalfd_siginfo info{};
        const ssize_t bytesRead = read(signalFd, &info, sizeof(info));
        if (bytesRead != static_cast<ssize_t>(sizeof(info))) {
            if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            if (bytesRead < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
        if (info.ssi_signo == SIGINT || info.ssi_signo == SIGTERM) {
            shutdown = true;
        } else if (info.ssi_signo == SIGCHLD) { //todo - get child pid to avoid the loop?
            workers.waitZombiesNoBlock();
        }
    }
}

// Returns whether this process is still the parent (false in the child after fork).
bool HandleClientConnections(pollfd& p, UniqueFd& listenSock, bool& shutdown, TScanConfig& scanConfig,
    TWorkerList& workers, std::vector<UniqueFd>& childReadEnds) {
    if (!(p.revents & POLLIN))
        return true;
    while (true) {
        UniqueFd client;
        try {
            client = listenSock.tryAcceptNonBlocking();
        } catch (const std::system_error &e) {
            const int ev = e.code().value();
            if (ev == EINVAL || ev == EBADF)
                shutdown = true;
            else
                throw;
            break;
        }
        if (!client)
            break;

        UniquePipe pipe;
        const pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) { // child
            pipe.readEnd.reset();
            listenSock.reset();
            WorkerMain(std::move(client), std::move(pipe.writeEnd), scanConfig);
            return false;
        } else {
            client.reset();
            pipe.writeEnd.reset();
            workers.addWorker(pid);
            childReadEnds.push_back(std::move(pipe.readEnd));
        }
    }
    return true;
}

void HandleChildReports(std::span<const pollfd> pf, TStatsStore& stats,
        std::vector<UniqueFd>& childReadEnds, std::unordered_map<int, std::string>& childBuffers) {
    for (const pollfd& p : pf) {
        if (p.revents & (POLLERR | POLLNVAL)) {
            childBuffers.erase(p.fd);
            RemoveChildReadFd(childReadEnds, p.fd);
            continue;
        }
        if (p.revents & (POLLIN | POLLHUP)) {
            std::string& buf = childBuffers[p.fd];
            bool got = false;
            char ch = 0;
            while (ReadChar(p.fd, ch)) {
                got = true;
                buf.push_back(ch);
                if (ch == '\n') {
                    std::string line;
                    if (buf.size() >= 1) {
                        line = buf.substr(0, buf.size() - 1);
                    }
                    buf.clear();
                    ApplyChildJsonLine(stats, line);
                }
            }
            if (!got && (p.revents & POLLHUP)) {
                if (!buf.empty()) {
                    ApplyChildJsonLine(stats, buf);
                    buf.clear();
                }
                childBuffers.erase(p.fd);
                RemoveChildReadFd(childReadEnds, p.fd);
            }
        }
    }
}


int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 5) {
            throw std::invalid_argument(
                std::string("Usage: ") + argv[0] + " <config.json> <port> [<fifo_request> <fifo_response>]");
        }

        std::string fifoRequestName;
        std::string fifoResponseName;
        if (argc == 5) {
            fifoRequestName = argv[3];
            fifoResponseName = argv[4];
        }

        TStatsStore stats;
        TScanConfig scanConfig;
        TWorkerList workers;
        //std::set<pid_t> workerPids;

        scanConfig = LoadScanConfigFromFile(argv[1]);
        if (argc == 3) {
            LoadFifoPathsFromConfigFile(argv[1], fifoRequestName, fifoResponseName);
        }

        if (fifoRequestName.empty() || fifoResponseName.empty()) {
            throw std::invalid_argument(
                "FIFO paths: set fifo_request and fifo_response in config or pass as extra arguments.");
        }

        std::uint16_t port = 0;
        {
            char* end = nullptr;
            const unsigned long v = std::strtoul(argv[2], &end, 10);
            if (end == argv[2] || *end != '\0' || v > 65535UL) {
                throw std::invalid_argument("Invalid port");
            }
            port = static_cast<std::uint16_t>(v);
        }

        makeFifoOrThrow(fifoRequestName, 0666);
        makeFifoOrThrow(fifoResponseName, 0666);

        UniqueFd listenSock = CreateListenSocketOrThrow(port);
        listenSock.setNonBlocking();

        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        sigaddset(&mask, SIGCHLD);
        if (int rc = pthread_sigmask(SIG_BLOCK, &mask, nullptr))
            throw std::system_error(rc, std::generic_category(), "pthread_sigmask");
        UniqueFd signalFd = UniqueFd::createForSignal(mask);        

        UniqueFd requestFd = UniqueFd::createForFifo(fifoRequestName.c_str(), O_RDWR);
        requestFd.setNonBlocking();

        std::unordered_map<int, std::string> childBuffers;
        std::vector<UniqueFd> workerDataToRead;
        bool shutdown = false;

        while (!shutdown) {
            std::vector<pollfd> pf; //fd order: signals, stat requests, client connections, workers' data
            pf.push_back({signalFd.get(), POLLIN, 0});
            pf.push_back({requestFd.get(), POLLIN, 0});
            pf.push_back({listenSock.get(), POLLIN, 0});
            for (const UniqueFd& workerFd : workerDataToRead) {
                pf.push_back({workerFd.get(), POLLIN, 0});
            }

            const int pr = poll(pf.data(), static_cast<nfds_t>(pf.size()), 500);
            if (pr < 0) {
                if (errno == EINTR)
                    continue;
                perror("poll");
                shutdown = true;
                break;
            }

            HandleSignal(pf[0], shutdown, listenSock, workers);

            HandleStatRequests(pf[1], stats, fifoResponseName);

            bool stillMainProcess = HandleClientConnections(pf[2], listenSock, shutdown, scanConfig, workers, workerDataToRead);
            if (!stillMainProcess)
                return 0;

            const std::span<const pollfd> childDataFds = std::span<const pollfd>(pf).subspan(3);
            HandleChildReports(childDataFds, stats, workerDataToRead, childBuffers);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}



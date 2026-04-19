#include "stats/stats_request.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

static bool WriteAll(int fd, const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        const ssize_t n = write(fd, data + sent, len - sent);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

static bool ReadAll(std::string& out, int fd) {
    char buf[4096];
    while (true) {
        const ssize_t n = read(fd, buf, sizeof(buf));
        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        out.append(buf, static_cast<std::size_t>(n));
    }
    return true;
}

void PrintStatsUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <fifo_request> <fifo_response>\n";
}

bool RequestStats(const std::string& fifoRequest, const std::string& fifoResponse, std::string& out) {
    out.clear();
    const int reqWr = open(fifoRequest.c_str(), O_WRONLY);
    if (reqWr < 0) {
        return false;
    }

    const char cmd[] = "\0";
    if (!WriteAll(reqWr, cmd, 1)) {
        close(reqWr);
        return false;
    }
    close(reqWr);

    const int respRd = open(fifoResponse.c_str(), O_RDONLY);
    if (respRd < 0) {
        return false;
    }

    if (!ReadAll(out, respRd)) {
        close(respRd);
        return false;
    }
    close(respRd);
    return true;
}

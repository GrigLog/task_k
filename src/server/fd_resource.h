#pragma once

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <string>
#include <system_error>


// Owns a single POSIX file descriptor; closes on destruction. Move-only.
class UniqueFd {
    int fd;
public:
    UniqueFd() noexcept : fd(-1) {
    }
    explicit UniqueFd(int fd) noexcept : fd(fd) {
    }
    ~UniqueFd() {
        reset();
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& o) noexcept : fd(o.fd) {
        o.fd = -1;
    }
    UniqueFd& operator=(UniqueFd&& o) noexcept {
        if (this != &o) {
            reset();
            fd = o.fd;
            o.fd = -1;
        }
        return *this;
    }

    int get() const noexcept {
        return fd;
    }
    explicit operator bool() const noexcept {
        return fd >= 0;
    }

    int release() noexcept {
        const int t = fd;
        fd = -1;
        return t;
    }

    void reset(int newFd = -1) noexcept {
        if (fd >= 0) {
            close(fd);
        }
        fd = newFd;
    }

    void setNonBlocking() {
        const int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            throw std::system_error(errno, std::generic_category(), "failed to get file descriptor flags");
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            throw std::system_error(errno, std::generic_category(), "failed to set file descriptor flags to non-blocking");
        }
    }

    static UniqueFd createForSignal(const sigset_t& mask) {
        const int fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "signalfd");
        }
        return UniqueFd(fd);
    }

    static UniqueFd createForFifo(const char* path, int oflags) {
        const int fd = open(path, oflags);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "open fifo_request");
        }
        return UniqueFd(fd);
    }

    // FIFO O_WRONLY|O_NONBLOCK: returns empty fd if no peer has opened the fifo for reading yet (ENXIO).
    // Does not block the event loop. Write end is switched back to blocking I/O for small payloads.
    static UniqueFd tryOpenFifoWriteNonBlocking(const char* path) {
        const int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (errno == ENXIO) {
                return UniqueFd{};
            }
            throw std::system_error(errno, std::generic_category(), "open fifo_response");
        }
        UniqueFd u(fd);
        const int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0 && (flags & O_NONBLOCK) != 0) {
            (void)fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        }
        return u;
    }

    // Called by listening socket. Returns empty UniqueFd on EAGAIN/EWOULDBLOCK; retries EINTR. Throws on other errors.
    UniqueFd tryAcceptNonBlocking() {
        while (true) {
            const int newFd = accept(fd, nullptr, nullptr);
            if (newFd >= 0) {
                return UniqueFd(newFd);
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return UniqueFd{};
            }
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "accept");
        }
    }
};

struct UniquePipe {
    UniqueFd readEnd;
    UniqueFd writeEnd;

    UniquePipe() {
        int fds[2];
        if (pipe(fds) != 0) {
            throw std::system_error(errno, std::generic_category(), "pipe");
        }
        readEnd = UniqueFd(fds[0]);
        writeEnd = UniqueFd(fds[1]);
    }
};


inline void makeFifoOrThrow(std::string path, mode_t mode) {
    if (mkfifo(path.c_str(), mode) != 0 && errno != EEXIST) {
        throw std::system_error(errno, std::generic_category(), std::string("mkfifo ") + path);
    }
}


#pragma once

#include "server/fd_resource.h"

#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <system_error>

inline UniqueFd CreateListenSocketOrThrow(std::uint16_t port) {
    const int raw = socket(AF_INET, SOCK_STREAM, 0);
    if (raw < 0) {
        throw std::system_error(errno, std::generic_category(), "socket");
    }
    UniqueFd fd(raw);

    int opt = 1;
    if (setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }
    if (listen(fd.get(), 128) < 0) {
        throw std::system_error(errno, std::generic_category(), "listen");
    }
    return fd;
}

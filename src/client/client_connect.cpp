#include "client/client_connect.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>

void PrintClientUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <file> <port>\n"
              << "       " << prog << " <file> <host> <port>\n";
}

bool ParsePort(const char* s, std::uint16_t& out) {
    char* end = nullptr;
    unsigned long v = std::strtoul(s, &end, 10);
    if (end == s || *end != '\0' || v > 65535UL) {
        return false;
    }
    out = static_cast<std::uint16_t>(v);
    return true;
}

int ConnectToHost(const char* host, std::uint16_t port) {
    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", static_cast<unsigned>(port));

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    const int gai = getaddrinfo(host, portStr, &hints, &res);
    if (gai != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(gai) << std::endl;
        return -1;
    }

    int sockFd = -1;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        sockFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockFd < 0) {
            continue;
        }
        if (connect(sockFd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(sockFd);
        sockFd = -1;
    }
    freeaddrinfo(res);
    if (sockFd < 0) {
        perror("connect");
    }
    return sockFd;
}

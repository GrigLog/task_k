#include "common/message.h"

#include <endian.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <limits>

static bool RecvAll(int fd, void* buf, size_t len) {
    auto* p = static_cast<unsigned char*>(buf);
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, p + got, len - got, 0);
        if (n <= 0) {
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

static bool SendAll(int fd, const void* buf, size_t len) {
    auto* p = static_cast<const unsigned char*>(buf);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool TMessage::ReadFromSocket(int socketFd, std::string& data) {
    std::uint64_t lengthNet = 0;
    if (!RecvAll(socketFd, &lengthNet, sizeof(lengthNet))) {
        return false;
    }
    std::uint64_t length = be64toh(lengthNet);
    if (length > static_cast<std::uint64_t>(std::numeric_limits<std::string::size_type>::max())) {
        return false;
    }
    const auto n = static_cast<std::string::size_type>(length);
    data.resize(n);
    if (n == 0) {
        return true;
    }
    return RecvAll(socketFd, data.data(), n);
}

bool TMessage::WriteToSocket(int socketFd, const std::string& data) {
    if (data.size() > std::numeric_limits<std::string::size_type>::max()) {
        return false;
    }
    const auto len = data.size();
    if (len > static_cast<size_t>(std::numeric_limits<std::uint64_t>::max())) {
        return false;
    }
    const std::uint64_t lengthHost = static_cast<std::uint64_t>(len);
    const std::uint64_t lengthNet = htobe64(lengthHost);
    if (!SendAll(socketFd, &lengthNet, sizeof(lengthNet))) {
        return false;
    }
    if (len == 0) {
        return true;
    }
    return SendAll(socketFd, data.data(), len);
}

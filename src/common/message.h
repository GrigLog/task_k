#pragma once

#include <string>

// Length-prefixed framing over a stream socket (or any connected SOCK_STREAM fd).
// Payload may contain embedded '\0' bytes (std::string holds arbitrary bytes).
// Wire format: 8-byte big-endian unsigned length (std::uint64_t) + payload bytes.

class TMessage {
public:
    static bool ReadFromSocket(int socketFd, std::string& data);
    static bool WriteToSocket(int socketFd, const std::string& data);
};

#include "client/client_connect.h"

#include "common/message.h"

#include <unistd.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    const char* filePath = nullptr;
    const char* host = "127.0.0.1";
    const char* portStr = nullptr;

    if (argc == 3) {
        filePath = argv[1];
        portStr = argv[2];
    } else if (argc == 4) {
        filePath = argv[1];
        host = argv[2];
        portStr = argv[3];
    } else {
        PrintClientUsage(argv[0]);
        return 1;
    }

    std::uint16_t port = 0;
    if (!ParsePort(portStr, port)) {
        std::cerr << "Invalid port: " << portStr << std::endl;
        return 1;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        perror("open");
        return 1;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    const int sockFd = ConnectToHost(host, port);
    if (sockFd < 0) {
        return 1;
    }

    if (!TMessage::WriteToSocket(sockFd, content)) {
        std::cerr << "Failed to send file to server" << std::endl;
        close(sockFd);
        return 1;
    }

    std::string reply;
    if (!TMessage::ReadFromSocket(sockFd, reply)) {
        std::cerr << "Failed to read reply from server" << std::endl;
        close(sockFd);
        return 1;
    }

    close(sockFd);
    std::cout << reply << std::endl;
    return 0;
}

#pragma once

#include <cstdint>

void PrintClientUsage(const char* prog);

bool ParsePort(const char* s, std::uint16_t& out);

// Returns connected socket fd, or -1 on failure.
int ConnectToHost(const char* host, std::uint16_t port);

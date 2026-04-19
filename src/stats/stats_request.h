#pragma once

#include <string>

void PrintStatsUsage(const char* prog);

// Writes one byte (value ignored) to fifo_request (O_WRONLY), reads full response from fifo_response (O_RDONLY).
// Returns true on success and sets out to the response body.
bool RequestStats(const std::string& fifoRequest, const std::string& fifoResponse, std::string& out);

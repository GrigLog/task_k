#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Configuration: category name -> list of substring patterns.
struct TScanConfig {
    std::map<std::string, std::vector<std::string>> Categories;
};

// Count non-overlapping occurrences of pattern in content (forward scan).
// Empty patterns are ignored (count 0).
std::uint64_t CountPatternOccurrences(const std::string& content, const std::string& pattern);

// Per category: sum of CountPatternOccurrences over all patterns in that category.
std::map<std::string, std::uint64_t> ScanContent(const std::string& content, const TScanConfig& config);

// Build compact JSON object string: {} or {"a":1,"b":2} (omit zero counts).
std::string CountsToJsonString(const std::map<std::string, std::uint64_t>& counts);

TScanConfig LoadScanConfigFromFile(const std::string& path);

// Optional fifo paths inside the same JSON config (keys fifo_request, fifo_response).
void LoadFifoPathsFromConfigFile(const std::string& path, std::string& fifoRequest, std::string& fifoResponse);


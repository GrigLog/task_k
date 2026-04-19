#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

// Aggregated server statistics for the stats utility.
// JSON shape: {"files": N, "patterns": {"cat": count, ...}}
class TStatsStore {
    std::uint64_t fileCount = 0;
    std::map<std::string, std::uint64_t> patternTotals;

public:
    void AddScanResult(const std::map<std::string, std::uint64_t>& fileCounts);

    std::uint64_t FileCount() const { return fileCount; }

    const std::map<std::string, std::uint64_t>& PatternTotals() const { return patternTotals; }

    nlohmann::json ToJson() const;
};

#include "server/stats_store.h"

void TStatsStore::AddScanResult(const std::map<std::string, std::uint64_t>& fileCounts) {
    ++fileCount;
    for (const auto& [k, v] : fileCounts) {
        patternTotals[k] += v;
    }
}

nlohmann::json TStatsStore::ToJson() const {
    nlohmann::json patterns = nlohmann::json::object();
    for (const auto& [k, v] : patternTotals) {
        patterns[k] = v;
    }
    nlohmann::json j;
    j["files"] = fileCount;
    j["patterns"] = std::move(patterns);
    return j;
}

#include "common/scanner.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

std::uint64_t CountPatternOccurrences(const std::string& content, const std::string& pattern) {
    if (pattern.empty()) {
        return 0;
    }
    std::uint64_t count = 0;
    std::size_t pos = 0;
    while (pos < content.size()) {
        const std::size_t found = content.find(pattern, pos);
        if (found == std::string::npos) {
            break;
        }
        ++count;
        pos = found + pattern.size();
    }
    return count;
}

std::map<std::string, std::uint64_t> ScanContent(const std::string& content, const TScanConfig& config) {
    std::map<std::string, std::uint64_t> out;
    for (const auto& [category, patterns] : config.Categories) {
        std::uint64_t sum = 0;
        for (const std::string& p : patterns) {
            sum += CountPatternOccurrences(content, p);
        }
        if (sum > 0) {
            out[category] = sum;
        }
    }
    return out;
}

std::string CountsToJsonString(const std::map<std::string, std::uint64_t>& counts) {
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [k, v] : counts) {
        j[k] = v;
    }
    return j.dump();
}

TScanConfig LoadScanConfigFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open config: " + path);
    }
    nlohmann::json j;
    in >> j;
    if (!j.is_object()) {
        throw std::runtime_error("config root must be a JSON object");
    }
    TScanConfig cfg;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string key = it.key();
        if (key == "fifo_request" || key == "fifo_response") {
            continue;
        }
        if (!it->is_array()) {
            continue;
        }
        std::vector<std::string> patterns;
        for (const auto& el : *it) {
            if (el.is_string()) {
                patterns.push_back(el.get<std::string>());
            }
        }
        cfg.Categories[key] = std::move(patterns);
    }
    return cfg;
}

void LoadFifoPathsFromConfigFile(const std::string& path, std::string& fifoRequest, std::string& fifoResponse) {
    fifoRequest.clear();
    fifoResponse.clear();
    std::ifstream in(path);
    if (!in) {
        return;
    }
    nlohmann::json j;
    in >> j;
    if (!j.is_object()) {
        return;
    }
    if (j.contains("fifo_request") && j["fifo_request"].is_string()) {
        fifoRequest = j["fifo_request"].get<std::string>();
    }
    if (j.contains("fifo_response") && j["fifo_response"].is_string()) {
        fifoResponse = j["fifo_response"].get<std::string>();
    }
}


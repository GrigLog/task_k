#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "server/stats_store.h"

TEST(StatsStore, ToJsonEmpty) {
    TStatsStore s;
    const nlohmann::json j = s.ToJson();
    EXPECT_EQ(j["files"], 0);
    EXPECT_TRUE(j["patterns"].is_object());
    EXPECT_TRUE(j["patterns"].empty());
}

TEST(StatsStore, MergesFilesAndPatterns) {
    TStatsStore s;
    s.AddScanResult({{"virus", 1}, {"ransomware", 3}});
    s.AddScanResult({{"virus", 1}});
    const nlohmann::json j = s.ToJson();
    EXPECT_EQ(j["files"], 2);
    EXPECT_EQ(j["patterns"]["virus"], 2);
    EXPECT_EQ(j["patterns"]["ransomware"], 3);
}

#include "common/scanner.h"

#include <gtest/gtest.h>

TEST(Scanner, CountPatternNonOverlapping) {
    EXPECT_EQ(CountPatternOccurrences("abab", "ab"), 2u);
    EXPECT_EQ(CountPatternOccurrences("aaaa", "aa"), 2u);
    EXPECT_EQ(CountPatternOccurrences("hello", "z"), 0u);
}

TEST(Scanner, ScanContentPerCategory) {
    TScanConfig cfg;
    cfg.Categories["virus"] = {"ab"};
    const std::string content = "xxabxxab";
    const auto m = ScanContent(content, cfg);
    ASSERT_EQ(m.count("virus"), 1u);
    EXPECT_EQ(m.at("virus"), 2u);
}

TEST(Scanner, EmptyJson) {
    std::map<std::string, std::uint64_t> empty;
    EXPECT_EQ(CountsToJsonString(empty), "{}");
}

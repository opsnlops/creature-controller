
#include <limits>

#include <gtest/gtest.h>

#include "util/string_utils.h"

TEST(StringUtils, stringToU32_ValidResult) {

    auto incomingString = "42";
    auto result = stringToU32(incomingString);
    EXPECT_EQ(result, 42);

}

TEST(StringUtils, stringToU32_NoneNumberInput) {

    auto incomingString = "poop";
    auto result = stringToU32(incomingString);
    EXPECT_EQ(result, 0);
}

TEST(StringUtils, stringToU32_BlankInput) {

    auto result = stringToU32("");
    EXPECT_EQ(result, 0);
}

TEST(StringUtils, stringToU32_TooBigInput) {

    u64 tooBig = std::numeric_limits<u64>::max();
    auto result = stringToU32(std::to_string(tooBig));
    EXPECT_EQ(result, 0);
}

TEST(StringUtils, stringToU32_LeadingTrailingSpaces) {
    EXPECT_EQ(stringToU32("  123  "), 123);
}

TEST(StringUtils, stringToU32_NumberPlusCharacters) {
    EXPECT_EQ(stringToU32("123abc"), 123);
}

TEST(StringUtils, stringToU32_NegativeNumber) {
    EXPECT_EQ(stringToU32("-69"), 0);
}

TEST(StringUtils, stringToU32_MaxUint32) {
    EXPECT_EQ(stringToU32("4294967295"), 4294967295); // max uint32_t
}

TEST(StringUtils, stringToU32_OverMaxUint32) {
    EXPECT_EQ(stringToU32("4294967296"), 0); // just over max uint32_t
}

TEST(StringUtils, stringToU32_HexadecimalInput) {
    EXPECT_EQ(stringToU32("0x1A"), 26);
}







TEST(StringUtils, splitString_BasicSplitting) {
    auto result = splitString("HEAP_FREE 209324");
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "HEAP_FREE");
    EXPECT_EQ(result[1], "209324");
}

TEST(StringUtils, splitString_EmptyString) {
    auto result = splitString("");
    EXPECT_TRUE(result.empty());
}

TEST(StringUtils, splitString_SingleWord) {
    auto result = splitString("Hello");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "Hello");
}

TEST(StringUtils, splitString_MultipleSpaces) {
    auto result = splitString("HEAP_FREE   209324");
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "HEAP_FREE");
    EXPECT_EQ(result[1], "209324");
}

TEST(StringUtils, splitString_LeadingTrailingSpaces) {
    auto result = splitString("  HEAP_FREE 209324  ");
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], "HEAP_FREE");
    EXPECT_EQ(result[1], "209324");
}

TEST(StringUtils, splitString_OnlyDelimiters) {
    auto result = splitString("   ");
    EXPECT_TRUE(result.empty());
}

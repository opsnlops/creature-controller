

#include <climits>
#include <cmath>

#include <gtest/gtest.h>

#include "controller-config.h"
#include "creature/DifferentialHead.h"
#include "mocks/logging/MockLogger.h"

using namespace creatures::creature;

class DifferentialHeadTest : public ::testing::Test {
  protected:
    std::shared_ptr<creatures::Logger> logger;
    // These match the current Parrot configuration:
    // headOffsetMaxPercent = 0.4, positionMin = 0, positionMax = 1023
    // headOffsetMax = lround(1023 * 0.4) = lround(409.2) = 409
    static constexpr float HEAD_OFFSET_MAX_PERCENT = 0.4f;
    static constexpr u16 POS_MIN = 0;
    static constexpr u16 POS_MAX = 1023;

    DifferentialHeadTest() : logger(std::make_shared<creatures::NiceMockLogger>()) {}

    DifferentialHead makeHead() { return DifferentialHead(logger, HEAD_OFFSET_MAX_PERCENT, POS_MIN, POS_MAX); }

    // Mirrors Creature::convertInputValueToServoValue for full-pipeline tests
    static u16 convertInputValueToServoValue(u8 inputValue) {
        u16 servoRange = MAX_POSITION - MIN_POSITION;
        double movementPercentage = (double)inputValue / (double)UCHAR_MAX;
        return (u16)(round((double)servoRange * movementPercentage) + MIN_POSITION);
    }
};

// ------------------------------------------------------------------
// convertToHeadHeight characterization tests
//
// Original Parrot math:
//   headOffsetMax = lround(1023 * 0.4) = 409
//   convertRange(y, 0, 1023, 0 + 409/2, 1023 - 409/2)
//   = convertRange(y, 0, 1023, 204, 818)
// ------------------------------------------------------------------

TEST_F(DifferentialHeadTest, ConvertToHeadHeight_Min) {
    auto head = makeHead();
    // convertRange(0, 0, 1023, 204, 818) = 204
    EXPECT_EQ(204, head.convertToHeadHeight(0));
}

TEST_F(DifferentialHeadTest, ConvertToHeadHeight_Mid) {
    auto head = makeHead();
    // convertRange(512, 0, 1023, 204, 818) = (512 * 614) / 1023 + 204 = 307 + 204 = 511
    EXPECT_EQ(511, head.convertToHeadHeight(512));
}

TEST_F(DifferentialHeadTest, ConvertToHeadHeight_Max) {
    auto head = makeHead();
    // convertRange(1023, 0, 1023, 204, 819) = 615 + 204 = 819
    EXPECT_EQ(819, head.convertToHeadHeight(1023));
}

// ------------------------------------------------------------------
// convertToHeadTilt characterization tests
//
// Original Parrot math (configToHeadTilt):
//   convertRange(x, 0, 1023, 1 - 409/2, 409/2)
//   = convertRange(x, 0, 1023, -203, 204)
// ------------------------------------------------------------------

TEST_F(DifferentialHeadTest, ConvertToHeadTilt_Min) {
    auto head = makeHead();
    // convertRange(0, 0, 1023, -203, 204) = -203
    EXPECT_EQ(-203, head.convertToHeadTilt(0));
}

TEST_F(DifferentialHeadTest, ConvertToHeadTilt_Mid) {
    auto head = makeHead();
    // convertRange(512, 0, 1023, -203, 204) = (512 * 407) / 1023 + (-203) = 203 - 203 = 0
    EXPECT_EQ(0, head.convertToHeadTilt(512));
}

TEST_F(DifferentialHeadTest, ConvertToHeadTilt_Max) {
    auto head = makeHead();
    // convertRange(1023, 0, 1023, -203, 204) = 407 - 203 = 204
    EXPECT_EQ(204, head.convertToHeadTilt(1023));
}

// ------------------------------------------------------------------
// calculateHeadPosition characterization tests
//
// Original Parrot math:
//   left = height - offset
//   right = height + offset
// ------------------------------------------------------------------

TEST_F(DifferentialHeadTest, CalculateHeadPosition_NoOffset) {
    auto head = makeHead();
    auto pos = head.calculateHeadPosition(511, 0);
    EXPECT_EQ(511, pos.left);
    EXPECT_EQ(511, pos.right);
}

TEST_F(DifferentialHeadTest, CalculateHeadPosition_PositiveOffset) {
    auto head = makeHead();
    auto pos = head.calculateHeadPosition(511, 100);
    EXPECT_EQ(411, pos.left);
    EXPECT_EQ(611, pos.right);
}

TEST_F(DifferentialHeadTest, CalculateHeadPosition_NegativeOffset) {
    auto head = makeHead();
    auto pos = head.calculateHeadPosition(511, -100);
    EXPECT_EQ(611, pos.left);
    EXPECT_EQ(411, pos.right);
}

// ------------------------------------------------------------------
// Full pipeline tests: DMX input (0-255) -> servo value -> head math
// This captures the exact Parrot behavior end-to-end
// ------------------------------------------------------------------

TEST_F(DifferentialHeadTest, FullPipeline_BothZero) {
    auto head = makeHead();

    u16 servoHeight = convertInputValueToServoValue(0); // 0
    u16 servoTilt = convertInputValueToServoValue(0);   // 0

    u16 headHeight = head.convertToHeadHeight(servoHeight); // 204
    int32_t headTilt = head.convertToHeadTilt(servoTilt);   // -203

    auto pos = head.calculateHeadPosition(headHeight, headTilt);
    // left = 204 - (-203) = 407
    // right = 204 + (-203) = 1
    EXPECT_EQ(407, pos.left);
    EXPECT_EQ(1, pos.right);
}

TEST_F(DifferentialHeadTest, FullPipeline_BothMax) {
    auto head = makeHead();

    u16 servoHeight = convertInputValueToServoValue(255); // 1023
    u16 servoTilt = convertInputValueToServoValue(255);   // 1023

    u16 headHeight = head.convertToHeadHeight(servoHeight); // 819
    int32_t headTilt = head.convertToHeadTilt(servoTilt);   // 204

    auto pos = head.calculateHeadPosition(headHeight, headTilt);
    // left = 819 - 204 = 615
    // right = 819 + 204 = 1023
    EXPECT_EQ(615, pos.left);
    EXPECT_EQ(1023, pos.right);
}

TEST_F(DifferentialHeadTest, FullPipeline_MidHeight_NoTilt) {
    auto head = makeHead();

    u16 servoHeight = convertInputValueToServoValue(128); // 514
    u16 servoTilt = convertInputValueToServoValue(0);     // 0

    u16 headHeight = head.convertToHeadHeight(servoHeight);
    int32_t headTilt = head.convertToHeadTilt(servoTilt); // -203

    auto pos = head.calculateHeadPosition(headHeight, headTilt);
    // headHeight = convertRange(514, 0, 1023, 204, 819) = (514*615)/1023 + 204 = 309 + 204 = 513
    // left = 513 - (-203) = 716
    // right = 513 + (-203) = 310
    EXPECT_EQ(716, pos.left);
    EXPECT_EQ(310, pos.right);
}

#include <sstream>
#include <gtest/gtest.h>
#include "config/UARTDevice.h"
#include "device/Servo.h"
#include "device/ServoException.h"
#include "device/ServoSpecifier.h"
#include "mocks/logging/MockLogger.h"

TEST(Servo, CreateServo) {
    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto location1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto servo = std::make_shared<Servo>(logger, "servoId", "Mock Servo", location1, 1000, 3000, 0.90, false, 50, 2000);

    float expectedSmoothingValue = 0.9f;
    float tolerance = 0.0001f;

    EXPECT_EQ("servoId", servo->getId());
    EXPECT_EQ("Mock Servo", servo->getName());
    EXPECT_EQ(1000, servo->getMinPulseUs());
    EXPECT_EQ(3000, servo->getMaxPulseUs());
    EXPECT_NEAR(expectedSmoothingValue, servo->getSmoothingValue(), tolerance);
    EXPECT_FALSE(servo->isInverted());
    EXPECT_EQ(2000, servo->getDefaultMicroseconds());
    EXPECT_EQ(50, servo->getServoUpdateFrequencyHz());
    EXPECT_EQ(20000, servo->getFrameLengthMicroseconds());
}

TEST(Servo, ServoOutOfRangeMove_Min) {
    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto location = ServoSpecifier(creatures::config::UARTDevice::B, 0);
    auto servo = std::make_shared<Servo>(logger, "mock", "Mock Server", location, 1000, 3000, 0.90, false, 50, 2000);

    auto result = servo->move(-1);

    EXPECT_FALSE(result.isSuccess());
    auto error = result.getError();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(creatures::ControllerError::InvalidData, error->getErrorType());
}

TEST(Servo, ServoOutOfRangeMove_Max) {
    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto location = ServoSpecifier(creatures::config::UARTDevice::C, 1);
    auto servo = std::make_shared<Servo>(logger, "mock", "Mock Server", location, 1000, 3000, 0.90, false, 50, 2000);

    auto result = servo->move(3001);

    EXPECT_FALSE(result.isSuccess());
    auto error = result.getError();
    ASSERT_TRUE(error.has_value());
    EXPECT_EQ(creatures::ControllerError::InvalidData, error->getErrorType());
}

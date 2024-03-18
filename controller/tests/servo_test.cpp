#include <sstream>

#include <gtest/gtest.h>

#include "config/UARTDevice.h"
#include "device/Servo.h"
#include "device/ServoException.h"
#include "device/ServoSpecifier.h"

#include "mocks/logging/MockLogger.h"

TEST(Servo, CreateServo) {

    /*
     * Servo(std::shared_ptr<creatures::Logger> logger, ServoSpecifier id, std::string name,
          u16 min_pulse_us, u16 max_pulse_us, float smoothingValue, bool inverted, u16 servo_update_frequency_hz,
          u16 default_position_microseconds);
     */


    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto location1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto servo = std::make_shared<Servo>(logger, "servoId", "Mock Servo", location1, 1000, 3000, 0.90, false, 50, 2000);

    // 🖕🏻 floating point!
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

    // This is a uint so this will actually be 65535
    EXPECT_THROW({servo->move(-1);
                 }, creatures::ServoException);

}

TEST(Servo, ServoOutOfRangeMove_Max) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto location = ServoSpecifier(creatures::config::UARTDevice::C, 1);
    auto servo = std::make_shared<Servo>(logger, "mock", "Mock Server", location, 1000, 3000, 0.90, false, 50, 2000);

    EXPECT_THROW({servo->move(3001);
                 }, creatures::ServoException);

}

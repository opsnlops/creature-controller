#include <sstream>

#include <gtest/gtest.h>

#include "device/servo.h"
#include "device/ServoException.h"


TEST(Servo, CreateServo) {

    auto servo = std::make_shared<Servo>("mock", 0, "Mock Server", 1000, 3000, 0.90, false, 2000);

    // 🖕🏻 floating point!
    float expectedSmoothingValue = 0.9f;
    float tolerance = 0.0001f;

    EXPECT_EQ("mock", servo->getId());
    EXPECT_EQ("Mock Server", servo->getName());
    EXPECT_EQ(1000, servo->getMinPulseUs());
    EXPECT_EQ(3000, servo->getMaxPulseUs());
    EXPECT_NEAR(expectedSmoothingValue, servo->getSmoothingValue(), tolerance);
    EXPECT_FALSE(servo->isInverted());
    EXPECT_EQ(2000, servo->getDefaultPosition());

}

TEST(Servo, ServoOutOfRangeMove_Min) {

    auto servo = std::make_shared<Servo>("mock", 0, "Mock Server", 1000, 3000, 0.90, false, 2000);

    // This is a uint so this will actually be 65535
    EXPECT_THROW({servo->move(-1);
                 }, creatures::ServoException);

}

TEST(Servo, ServoOutOfRangeMove_Max) {

    auto servo = std::make_shared<Servo>("mock", 0, "Mock Server", 1000, 3000, 0.90, false, 2000);

    EXPECT_THROW({servo->move(3001);
                 }, creatures::ServoException);

}

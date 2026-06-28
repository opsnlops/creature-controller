#include <cstdint>
#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "config/UARTDevice.h"
#include "controller/Controller.h"
#include "controller/commands/ServoModuleConfiguration.h"
#include "creature/MotorType.h"
#include "device/Servo.h"
#include "device/ServoSpecifier.h"
#include "io/MessageRouter.h"

#include "mocks/creature/MockCreature.h"
#include "mocks/logging/MockLogger.h"

using creatures::config::UARTDevice;
using creatures::creature::motor_type;

// Verifies that the controller only sends a motor type to firmware that can
// actually drive it. Dynamixel support arrived in firmware version 4 (HW4);
// version 3 (HW3) is standard-servo only.
class ServoModuleConfigurationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        logger = std::make_shared<creatures::NiceMockLogger>();
        messageRouter = std::make_shared<creatures::io::MessageRouter>(logger);
        creature = std::make_shared<MockCreature>(logger);
        controller = std::make_shared<Controller>(logger, creature, messageRouter);
    }

    // Add a servo of the given type to module A of the creature.
    void addServo(const std::string &id, uint16_t pin, motor_type type) {
        auto location = ServoSpecifier(UARTDevice::A, pin, type);
        auto servo = std::make_shared<Servo>(logger, id, id, location, 1000, 3000, 0.90f, false, 50, 2000);
        creature->addServo(id, servo);
    }

    std::shared_ptr<creatures::NiceMockLogger> logger;
    std::shared_ptr<creatures::io::MessageRouter> messageRouter;
    std::shared_ptr<MockCreature> creature;
    std::shared_ptr<Controller> controller;
};

TEST_F(ServoModuleConfigurationTest, RejectsDynamixelOnVersion3) {
    addServo("neck", 1, motor_type::dynamixel);

    creatures::commands::ServoModuleConfiguration config(logger);
    auto result = config.getServoConfigurations(controller, UARTDevice::A, 3);

    EXPECT_FALSE(result.isSuccess());
}

TEST_F(ServoModuleConfigurationTest, AcceptsDynamixelOnVersion4) {
    addServo("neck", 1, motor_type::dynamixel);

    creatures::commands::ServoModuleConfiguration config(logger);
    auto result = config.getServoConfigurations(controller, UARTDevice::A, 4);

    ASSERT_TRUE(result.isSuccess());
    EXPECT_NE(config.toMessage().find("DYNAMIXEL"), std::string::npos);
}

TEST_F(ServoModuleConfigurationTest, AcceptsStandardServoOnVersion3) {
    addServo("jaw", 0, motor_type::servo);

    creatures::commands::ServoModuleConfiguration config(logger);
    auto result = config.getServoConfigurations(controller, UARTDevice::A, 3);

    ASSERT_TRUE(result.isSuccess());
    const auto message = config.toMessage();
    EXPECT_EQ(message.find("DYNAMIXEL"), std::string::npos);
    EXPECT_NE(message.find("SERVO"), std::string::npos);
}

// A module that mixes standard and Dynamixel servos must still be rejected on
// HW3 - the whole creature can't run there.
TEST_F(ServoModuleConfigurationTest, RejectsMixedConfigOnVersion3) {
    addServo("jaw", 0, motor_type::servo);
    addServo("neck", 1, motor_type::dynamixel);

    creatures::commands::ServoModuleConfiguration config(logger);
    auto result = config.getServoConfigurations(controller, UARTDevice::A, 3);

    EXPECT_FALSE(result.isSuccess());
}

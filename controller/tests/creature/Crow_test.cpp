
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "creature/Crow.h"
#include "creature/MotorType.h"
#include "device/Servo.h"
#include "device/ServoSpecifier.h"

#include "mocks/logging/MockLogger.h"

using creatures::creature::motor_type;

class CrowTest : public ::testing::Test {
  protected:
    const std::vector<std::string> axes = {"lean", "beak", "neck_twist", "head_twist", "neck_lean", "wings"};

    std::shared_ptr<creatures::Logger> logger = std::make_shared<creatures::NiceMockLogger>();
    Crow crow{logger};

    void addMotor(const std::string &id, u16 busId, motor_type type = motor_type::dynamixel, bool inverted = false) {
        auto output = ServoSpecifier(creatures::config::UARTDevice::A, busId, type);
        crow.addServo(id, std::make_shared<Servo>(logger, id, id, output, 1000, 3000, 0.9f, inverted, 50, 2000));
    }

    void addAllMotors() {
        u16 busId = 1;
        for (const auto &axis : axes) {
            addMotor(axis, busId++);
        }
    }

    static std::unordered_map<std::string, creatures::Input> makeInputs(const std::vector<std::string> &names,
                                                                        u32 value) {
        std::unordered_map<std::string, creatures::Input> inputs;
        u16 slot = 0;
        for (const auto &name : names) {
            inputs.emplace(name, creatures::Input(name, slot++, 1, value));
        }
        return inputs;
    }

    // Where the motor will actually be told to go on the next frame
    u32 nextPosition(const std::string &id) {
        crow.calculateNextServoPositions();
        return crow.getServo(id)->getCurrentMicroseconds();
    }
};

TEST_F(CrowTest, PreFlightPassesWithSixDynamixels) {
    addAllMotors();
    EXPECT_TRUE(crow.performPreFlightCheck().isSuccess());
}

TEST_F(CrowTest, PreFlightFailsWhenAnyAxisIsMissing) {
    for (const auto &missing : axes) {
        Crow incomplete(logger);
        u16 busId = 1;
        for (const auto &axis : axes) {
            if (axis == missing) {
                continue;
            }
            auto output = ServoSpecifier(creatures::config::UARTDevice::A, busId++, motor_type::dynamixel);
            incomplete.addServo(axis,
                                std::make_shared<Servo>(logger, axis, axis, output, 1000, 3000, 0.9f, false, 50, 2000));
        }
        EXPECT_FALSE(incomplete.performPreFlightCheck().isSuccess()) << missing;
    }
}

TEST_F(CrowTest, PreFlightFailsWithAPwmServo) {
    for (u16 i = 0; i < 5; i++) {
        addMotor(axes[i], i + 1);
    }
    addMotor("wings", 6, motor_type::servo);

    EXPECT_FALSE(crow.performPreFlightCheck().isSuccess());
}

TEST_F(CrowTest, EachInputMovesOnlyItsOwnMotor) {
    addAllMotors();

    for (const auto &moved : axes) {
        crow.mapInputsToServos(makeInputs(axes, 0));
        crow.mapInputsToServos(makeInputs({moved}, 255));

        for (const auto &axis : axes) {
            EXPECT_EQ(axis == moved ? 3000u : 1000u, nextPosition(axis)) << "moved " << moved << ", checked " << axis;
        }
    }
}

TEST_F(CrowTest, PassesTheFullRangeThrough) {
    addAllMotors();

    u32 previous = 0;
    for (u32 value = 0; value <= 255; value++) {
        crow.mapInputsToServos(makeInputs(axes, value));

        // Same input, same range, so every axis has to agree
        u32 expected = nextPosition(axes.front());
        for (const auto &axis : axes) {
            EXPECT_EQ(expected, nextPosition(axis)) << axis << " at " << value;
        }
        EXPECT_GT(expected, previous) << "at " << value;
        EXPECT_NEAR(1000.0 + value * 2000.0 / 255.0, expected, 3.0);
        previous = expected;
    }
}

TEST_F(CrowTest, InvertedMotorsAreStillInverted) {
    for (u16 i = 0; i < 5; i++) {
        addMotor(axes[i], i + 1);
    }
    addMotor("wings", 6, motor_type::dynamixel, true);

    crow.mapInputsToServos(makeInputs(axes, 0));
    EXPECT_EQ(3000u, nextPosition("wings"));
    EXPECT_EQ(1000u, nextPosition("lean"));
}

TEST_F(CrowTest, MissingInputLeavesThatMotorAlone) {
    addAllMotors();
    crow.mapInputsToServos(makeInputs(axes, 0));

    EXPECT_NO_THROW(crow.mapInputsToServos(makeInputs({"lean", "beak"}, 255)));

    EXPECT_EQ(3000u, nextPosition("lean"));
    EXPECT_EQ(3000u, nextPosition("beak"));
    EXPECT_EQ(1000u, nextPosition("neck_twist"));
}

TEST_F(CrowTest, ExtraInputsAreIgnored) {
    addAllMotors();
    crow.mapInputsToServos(makeInputs(axes, 0));

    EXPECT_NO_THROW(crow.mapInputsToServos(makeInputs({"chest"}, 255)));
    for (const auto &axis : axes) {
        EXPECT_EQ(1000u, nextPosition(axis)) << axis;
    }
}

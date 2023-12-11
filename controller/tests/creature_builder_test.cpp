//
// Created by April White on 12/10/23.
//

#include <sstream>

#include <gtest/gtest.h>

#include "config/creature_builder.h"
#include "config/CreatureBuilderException.h"


TEST(CreatureBuilder, BuildsCorrectlyWithValidData) {
    const std::string validJsonData = R"({  "name": "Test Creature",
      "version": "0.1.0",
      "description": "This is a fake creature for testing",
      "starting_dmx_channel": 1,
      "position_min": 0,
      "position_max": 1023,
      "head_offset_max": 0.4,
      "servo_frequency": 50,
      "type": "parrot",
      "motors": [
        {
          "type": "servo",
          "id": "neck_left",
          "name": "Neck Left",
          "output_pin": 0,
          "min_pulse_us": 1250,
          "max_pulse_us": 2250,
          "smoothing_value": 0.90,
          "inverted": false,
          "default_position": "center"
        }
      ],
      "inputs": [

      ]}
    )";
    auto jsonStream = std::make_unique<std::istringstream>(validJsonData);

    creatures::CreatureBuilder builder(std::move(jsonStream));
    auto creature = builder.build();

    // 😡 floating point!
    float expectedHeadOffsetMax = 0.4f;
    float expectedSmoothingValue = 0.9f;
    float tolerance = 0.0001f;

    // Assertions to validate the built creature
    EXPECT_EQ("Test Creature", creature->getName());
    EXPECT_EQ("This is a fake creature for testing", creature->getDescription());
    EXPECT_EQ(1, creature->getStartingDmxChannel());
    EXPECT_EQ(0, creature->getPositionMin());
    EXPECT_EQ(1023, creature->getPositionMax());
    EXPECT_NEAR(expectedHeadOffsetMax, creature->getHeadOffsetMax(), tolerance);
    EXPECT_EQ(50, creature->getServoFrequency());
    EXPECT_EQ(1, creature->getNumberOfServos());
    EXPECT_EQ("neck_left", creature->getServo("neck_left")->getId());
    EXPECT_EQ("Neck Left", creature->getServo("neck_left")->getName());
    EXPECT_EQ(0, creature->getServo("neck_left")->getOutputPin());
    EXPECT_EQ(1250, creature->getServo("neck_left")->getMinPulseUs());
    EXPECT_EQ(2250, creature->getServo("neck_left")->getMaxPulseUs());
    EXPECT_NEAR(expectedSmoothingValue, creature->getServo("neck_left")->getSmoothingValue(), tolerance);
    EXPECT_FALSE(creature->getServo("neck_left")->isInverted());
    EXPECT_EQ(1250 + ((2250 - 1250) / 2),  creature->getServo("neck_left")->getDefaultPosition());

    //EXPECT_EQ(0, creature->getNumberOfInputs());

}

TEST(CreatureBuilder, BuildFails_EmptyJson) {
    const std::string badJsonData = R"({})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_BrokenJson) {
    const std::string badJsonData = R"({"type: "parrot"})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_MeaningLessJson) {
    const std::string badJsonData = R"({"type": "parrot", "name": 42})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_InvalidType) {
    const std::string badJsonData = R"({"type": "poop", "name": "Beaky"})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_MissingMotors) {
    const std::string badJsonData = R"({  "name": "Test Creature",
      "version": "0.1.0",
      "description": "This is a fake creature for testing",
      "starting_dmx_channel": 1,
      "position_min": 0,
      "position_max": 666,
      "head_offset_max": 0.4,
      "servo_frequency": 50,
      "type": "parrot"
      }
    )";

    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}




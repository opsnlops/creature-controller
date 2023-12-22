

#include <sstream>

#include <gtest/gtest.h>
#include <gmock/gmock.h>


#include "config/CreatureBuilder.h"
#include "config/CreatureBuilderException.h"
#include "mocks/logging/MockLogger.h"


TEST(CreatureBuilder, BuildsCorrectlyWithValidData) {

    std::shared_ptr<creatures::Logger> logger = std::make_shared<creatures::NiceMockLogger>();
    logger->debug("Starting test");

    const std::string validJsonData = R"({  "name": "Test Creature",
      "version": "0.1.0",
      "description": "This is a fake creature for testing",
      "starting_dmx_channel": 1,
      "position_min": 0,
      "position_max": 1023,
      "head_offset_max": 0.4,
      "frame_time_ms": 20,
      "type": "parrot",
      "motors": [
        {
          "type": "servo",
          "id": "neck_left",
          "name": "Neck Left",
          "output_location": "A0",
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

    creatures::CreatureBuilder builder(logger, std::move(jsonStream));
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
    EXPECT_EQ(20, creature->getFrameTimeMs());
    EXPECT_EQ(1, creature->getNumberOfServos());
    EXPECT_EQ("neck_left", creature->getServo("neck_left")->getId());
    EXPECT_EQ("Neck Left", creature->getServo("neck_left")->getName());
    EXPECT_EQ("A0", creature->getServo("neck_left")->getOutputLocation());
    EXPECT_EQ(1250, creature->getServo("neck_left")->getMinPulseUs());
    EXPECT_EQ(2250, creature->getServo("neck_left")->getMaxPulseUs());
    EXPECT_NEAR(expectedSmoothingValue, creature->getServo("neck_left")->getSmoothingValue(), tolerance);
    EXPECT_FALSE(creature->getServo("neck_left")->isInverted());
    EXPECT_EQ(1250 + ((2250 - 1250) / 2),  creature->getServo("neck_left")->getDefaultPosition());

    //EXPECT_EQ(0, creature->getNumberOfInputs());

}

TEST(CreatureBuilder, BuildFails_EmptyJson) {

   auto logger = std::make_shared<creatures::NiceMockLogger>();

    const std::string badJsonData = R"({})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);


    EXPECT_THROW({
                     creatures::CreatureBuilder builder(logger, std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_BrokenJson) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    const std::string badJsonData = R"({"type: "parrot"})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
;

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(logger, std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_MeaningLessJson) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    const std::string badJsonData = R"({"type": "parrot", "name": 42})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);


    EXPECT_THROW({
                     creatures::CreatureBuilder builder(logger, std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_InvalidType) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    const std::string badJsonData = R"({"type": "poop", "name": "Beaky"})";
    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(logger, std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}

TEST(CreatureBuilder, BuildFails_MissingMotors) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();


    const std::string badJsonData = R"({  "name": "Test Creature",
      "version": "0.1.0",
      "description": "This is a fake creature for testing",
      "starting_dmx_channel": 1,
      "position_min": 0,
      "position_max": 666,
      "head_offset_max": 0.4,
      "frame_time_ms": 20,
      "type": "parrot"
      }
    )";

    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);

    EXPECT_THROW({
                     creatures::CreatureBuilder builder(logger, std::move(jsonStream));
                     builder.build();
                 }, creatures::CreatureBuilderException);

}




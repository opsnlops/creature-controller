

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <filesystem>
#include <random>
#include <sstream>


#include "config/BuilderException.h"
#include "config/CreatureBuilder.h"
#include "config/CreatureBuilderException.h"
#include "mocks/logging/MockLogger.h"

namespace {
    std::string CreateSecureTempFileWithContent(const std::string& content) {
        std::random_device rd;
        std::default_random_engine generator(rd());
        std::uniform_int_distribution<int> distribution(0, 999999);
        auto randomSuffix = distribution(generator);

        auto tempPath = std::filesystem::temp_directory_path() / ("creature_temp_" + std::to_string(randomSuffix) + ".json");
        std::ofstream outFile(tempPath);
        outFile << content;
        outFile.close();
        return tempPath.string();
    }
}


class CreatureBuilderTest : public ::testing::Test {
protected:
    std::shared_ptr<creatures::Logger> logger;
    std::string tempValidFileName;

    CreatureBuilderTest() : logger(std::make_shared<creatures::NiceMockLogger>()) {}

    void SetUp() override {
        const std::string validJsonData = R"({  "name": "Test Creature",
      "version": "0.1.0",
      "description": "This is a fake creature for testing",
      "channel_offset": 1,
      "universe": 234,
      "position_min": 0,
      "position_max": 1023,
      "head_offset_max": 0.4,
      "type": "parrot",
      "servo_frequency": 50,
      "motors": [
        {
          "type": "servo",
          "id": "neck_left",
          "name": "Neck Left",
          "output_module": "A",
          "output_header": 0,
          "min_pulse_us": 1250,
          "max_pulse_us": 2250,
          "smoothing_value": 0.90,
          "inverted": false,
          "default_position": "center"
        }
      ],
      "inputs": [
        {
          "name": "head_tilt",
          "slot": 0,
          "width": 1
        },
        {
          "name": "head_height",
          "slot": 1,
          "width": 1
        },
        {
          "name": "neck_rotate",
          "slot": 2,
          "width": 1
        }
      ]}
    )";

        tempValidFileName = CreateSecureTempFileWithContent(validJsonData);
    }

    void TearDown() override {
        if (!tempValidFileName.empty()) {
            std::filesystem::remove(tempValidFileName); // Clean up the temp file
        }
    }

    void CreateTempFileWithJson(const std::string& jsonData) {
        tempValidFileName = CreateSecureTempFileWithContent(jsonData);
    }
};


TEST_F(CreatureBuilderTest, BuildsCorrectlyWithValidData) {

    std::shared_ptr<creatures::Logger> logger = std::make_shared<creatures::NiceMockLogger>();
    logger->debug("Starting test");


    creatures::config::CreatureBuilder builder(logger, tempValidFileName);
    auto creatureResult = builder.build();
    EXPECT_TRUE(creatureResult.isSuccess());

    auto creature = creatureResult.getValue().value();

    // 😡 floating point!
    float expectedHeadOffsetMax = 0.4f;
    float expectedSmoothingValue = 0.9f;
    float tolerance = 0.0001f;


    auto properLocation = ServoSpecifier(creatures::config::UARTDevice::A, 0);

    // Assertions to validate the built creature
    EXPECT_EQ("Test Creature", creature->getName());
    EXPECT_EQ("This is a fake creature for testing", creature->getDescription());
    EXPECT_EQ(1, creature->getChannelOffset());
    EXPECT_EQ(234, creature->getUniverse());
    EXPECT_EQ(0, creature->getPositionMin());
    EXPECT_EQ(1023, creature->getPositionMax());
    EXPECT_NEAR(expectedHeadOffsetMax, creature->getHeadOffsetMax(), tolerance);
    EXPECT_EQ(50, creature->getServoUpdateFrequencyHz());
    EXPECT_EQ(1, creature->getNumberOfServos());
    EXPECT_EQ("neck_left", creature->getServo("neck_left")->getId());
    EXPECT_EQ("Neck Left", creature->getServo("neck_left")->getName());
    EXPECT_EQ(properLocation, creature->getServo("neck_left")->getOutputLocation());
    EXPECT_EQ(1250, creature->getServo("neck_left")->getMinPulseUs());
    EXPECT_EQ(2250, creature->getServo("neck_left")->getMaxPulseUs());
    EXPECT_NEAR(expectedSmoothingValue, creature->getServo("neck_left")->getSmoothingValue(), tolerance);
    EXPECT_FALSE(creature->getServo("neck_left")->isInverted());
    EXPECT_EQ(1250 + ((2250 - 1250) / 2), creature->getServo("neck_left")->getDefaultMicroseconds());

}

//TEST(CreatureBuilder, BuildFails_EmptyJson) {
//
//   auto logger = std::make_shared<creatures::NiceMockLogger>();
//
//    const std::string badJsonData = R"({})";
//    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
//
//
//    EXPECT_THROW({
//                     creatures::config::CreatureBuilder builder(logger, std::move(jsonStream));
//                     builder.build();
//                 }, creatures::BuilderException);
//
//}
//
//TEST(CreatureBuilder, BuildFails_BrokenJson) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//
//    const std::string badJsonData = R"({"type: "parrot"})";
//    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
//
//
//    EXPECT_THROW({
//                     creatures::config::CreatureBuilder builder(logger, std::move(jsonStream));
//                     builder.build();
//                 }, creatures::CreatureBuilderException);
//
//}
//
//TEST(CreatureBuilder, BuildFails_MeaningLessJson) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//
//    const std::string badJsonData = R"({"type": "parrot", "name": 42})";
//    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
//
//
//    EXPECT_THROW({
//                     creatures::config::CreatureBuilder builder(logger, std::move(jsonStream));
//                     builder.build();
//                 }, creatures::BuilderException);
//
//}
//
//TEST(CreatureBuilder, BuildFails_InvalidType) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//
//    const std::string badJsonData = R"({"type": "poop", "name": "Beaky"})";
//    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
//
//    EXPECT_THROW({
//                     creatures::config::CreatureBuilder builder(logger, std::move(jsonStream));
//                     builder.build();
//                 }, creatures::BuilderException);
//
//}
//
//TEST(CreatureBuilder, BuildFails_MissingMotors) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//
//    const std::string badJsonData = R"({  "name": "Test Creature",
//    "version": "0.1.0",
//    "description": "This is a fake creature for testing",
//    "starting_dmx_channel": 1,
//    "dmx_universe": 234,
//    "position_min": 0,
//    "position_max": 666,
//    "head_offset_max": 0.4,
//    "servo_frequency": 50,
//    "type": "parrot",
//    "inputs": [
//        {
//            "name": "left",
//            "slot": 1,
//            "width": 2
//        }
//    ]
//    }
//    )";
//
//    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
//
//    EXPECT_THROW({
//                     creatures::config::CreatureBuilder builder(logger, std::move(jsonStream));
//                     builder.build();
//                 }, creatures::BuilderException);
//
//}
//
//TEST(CreatureBuilder, BuildFails_MissingInputs) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//
//    const std::string badJsonData = R"({  "name": "Test Creature",
//        "version": "0.1.0",
//        "description": "This is a fake creature for testing",
//        "channel_offset": 1,
//        "universe": 234,
//        "position_min": 0,
//        "position_max": 666,
//        "head_offset_max": 0.4,
//        "servo_frequency": 50,
//        "type": "parrot",
//        "motors": [
//                {
//                  "type": "servo",
//                  "id": "neck_left",
//                  "name": "Neck Left",
//                  "output_location": "A0",
//                  "min_pulse_us": 1250,
//                  "max_pulse_us": 2250,
//                  "smoothing_value": 0.90,
//                  "inverted": false,
//                  "default_position": "center"
//                }
//              ],
//      }
//    )";
//
//    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
//
//    EXPECT_THROW({
//                     creatures::config::CreatureBuilder builder(logger, std::move(jsonStream));
//                     builder.build();
//                 }, creatures::CreatureBuilderException);
//
//}
//
//
//TEST(CreatureBuilder, BuildFails_InputSlotOutOfRange) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//
//    const std::string badJsonData = R"({  "name": "Test Creature",
//        "version": "0.1.0",
//        "description": "This is a fake creature for testing",
//        "channel_offset": 1,
//        "universe": 234,
//        "position_min": 0,
//        "position_max": 666,
//        "head_offset_max": 0.4,
//        "servo_frequency": 50,
//        "type": "parrot",
//        "motors": [
//                {
//                  "type": "servo",
//                  "id": "neck_left",
//                  "name": "Neck Left",
//                  "output_location": "A0",
//                  "min_pulse_us": 1250,
//                  "max_pulse_us": 2250,
//                  "smoothing_value": 0.90,
//                  "inverted": false,
//                  "default_position": "center"
//                }
//              ],
//        "inputs": [
//        {
//            "name": "up",
//            "slot": 513,
//            "width": 1
//        }
//    ]
//      }
//    )";
//
//    auto jsonStream = std::make_unique<std::istringstream>(badJsonData);
//
//    EXPECT_THROW({
//                     creatures::config::CreatureBuilder builder(logger, std::move(jsonStream));
//                     builder.build();
//                 }, creatures::CreatureBuilderException);
//
//}
//
//

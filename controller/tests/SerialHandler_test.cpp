

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "io/SerialHandler.h"
#include "io/SerialException.h"
#include "util/MessageQueue.h"


#include "mocks/logging/MockLogger.h"


TEST(SerialOutput, CreateSerialOutput_ValidDevice) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_NO_THROW({
        auto serialOutput = creatures::SerialHandler(logger, "/dev/null", outputQueue, inputQueue);
    });

}

TEST(SerialOutput, CreateSerialOutput_DeviceDoesNotExist) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
        auto serialOutput = creatures::SerialHandler(logger, "", outputQueue, inputQueue);
    }, creatures::SerialException);
}

TEST(SerialOutput, CreateSerialOutput_DeviceNotCharacterDevice) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
                     auto serialOutput = creatures::SerialHandler(logger, "/", outputQueue, inputQueue);
                 }, creatures::SerialException);
}

TEST(SerialOutput, CreateSerialOutput_InvalidOutputQueue) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
                     auto serialOutput = creatures::SerialHandler(logger, "/dev/null", nullptr, inputQueue);
                 }, creatures::SerialException);
}

TEST(SerialOutput, CreateSerialOutput_InvalidInputQueue) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
                     auto serialOutput = creatures::SerialHandler(logger, "/dev/null", outputQueue, nullptr);
                 }, creatures::SerialException);
}

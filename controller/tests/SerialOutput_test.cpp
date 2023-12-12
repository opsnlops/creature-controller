

#include <gtest/gtest.h>


#include "io/SerialOutput.h"
#include "io/SerialException.h"
#include "util/MessageQueue.h"


TEST(SerialOutput, CreateSerialOutput_ValidDevice) {

    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_NO_THROW({
        auto serialOutput = creatures::SerialOutput("/dev/null", outputQueue, inputQueue);
    });

}

TEST(SerialOutput, CreateSerialOutput_DeviceDoesNotExist) {

    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
        auto serialOutput = creatures::SerialOutput("", outputQueue, inputQueue);
    }, creatures::SerialException);
}

TEST(SerialOutput, CreateSerialOutput_DeviceNotCharacterDevice) {

    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
                     auto serialOutput = creatures::SerialOutput("/", outputQueue, inputQueue);
                 }, creatures::SerialException);
}

TEST(SerialOutput, CreateSerialOutput_InvalidOutputQueue) {

    auto inputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
                     auto serialOutput = creatures::SerialOutput("/dev/null", nullptr, inputQueue);
                 }, creatures::SerialException);
}

TEST(SerialOutput, CreateSerialOutput_InvalidInputQueue) {

    auto outputQueue = std::make_shared<creatures::MessageQueue<std::string>>();

    EXPECT_THROW({
                     auto serialOutput = creatures::SerialOutput("/dev/null", outputQueue, nullptr);
                 }, creatures::SerialException);
}

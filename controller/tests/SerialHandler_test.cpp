#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "io/Message.h"
#include "io/SerialHandler.h"
#include "io/SerialException.h"
#include "util/MessageQueue.h"
#include "mocks/logging/MockLogger.h"

extern std::atomic<bool> shutdown_requested;

namespace creatures {

    // There isn't a clean way to mock a serial device in a unit test
//    TEST(SerialOutput, CreateSerialOutput_ValidDevice) {
//        auto logger = std::make_shared<NiceMockLogger>();
//        auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
//        auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
//
//        EXPECT_NO_THROW({
//                            auto serialOutput = SerialHandler(logger, "/dev/null", UARTDevice::A, outputQueue, inputQueue);
//                        });
//    }

    TEST(SerialOutput, CreateSerialOutput_DeviceDoesNotExist) {
        auto logger = std::make_shared<NiceMockLogger>();
        auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
        auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

        EXPECT_THROW({
                         auto serialOutput = SerialHandler(logger, "", UARTDevice::A, outputQueue, inputQueue);
                     }, SerialException);
    }

    TEST(SerialOutput, CreateSerialOutput_DeviceNotCharacterDevice) {
        auto logger = std::make_shared<NiceMockLogger>();
        auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
        auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

        EXPECT_THROW({
                         auto serialOutput = SerialHandler(logger, "/", UARTDevice::A, outputQueue, inputQueue);
                     }, SerialException);
    }

    TEST(SerialOutput, CreateSerialOutput_InvalidOutputQueue) {
        auto logger = std::make_shared<NiceMockLogger>();
        auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

        EXPECT_THROW({
                         auto serialOutput = SerialHandler(logger, "/dev/null", UARTDevice::A, nullptr, inputQueue);
                     }, SerialException);
    }

    TEST(SerialOutput, CreateSerialOutput_InvalidInputQueue) {
        auto logger = std::make_shared<NiceMockLogger>();
        auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

        EXPECT_THROW({
                         auto serialOutput = SerialHandler(logger, "/dev/null", UARTDevice::A, outputQueue, nullptr);
                     }, SerialException);
    }

} // namespace creatures

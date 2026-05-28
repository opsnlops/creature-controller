#include <stdexcept>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "io/Message.h"
#include "io/SerialHandler.h"
#include "mocks/logging/MockLogger.h"
#include "util/MessageQueue.h"

extern std::atomic<bool> shutdown_requested;

namespace creatures {

// There isn't a clean way to mock a serial device in a unit test
//    TEST(SerialOutput, CreateSerialOutput_ValidDevice) {
//        auto logger = std::make_shared<NiceMockLogger>();
//        auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
//        auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
//
//        EXPECT_NO_THROW({
//                            auto serialOutput = SerialHandler(logger, "/dev/null", UARTDevice::A, outputQueue,
//                            inputQueue);
//                        });
//    }

// Bad device nodes are no longer fatal at construction time; the constructor logs and
// the caller learns about the failure when start() / setupSerialPort() returns an error
// Result. These tests pin that contract.
TEST(SerialOutput, CreateSerialOutput_DeviceDoesNotExist_IsNonFatal) {
    auto logger = std::make_shared<NiceMockLogger>();
    auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
    auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

    EXPECT_NO_THROW({ auto serialOutput = SerialHandler(logger, "", UARTDevice::A, outputQueue, inputQueue); });
}

TEST(SerialOutput, CreateSerialOutput_DeviceNotCharacterDevice_IsNonFatal) {
    auto logger = std::make_shared<NiceMockLogger>();
    auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();
    auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

    EXPECT_NO_THROW({ auto serialOutput = SerialHandler(logger, "/", UARTDevice::A, outputQueue, inputQueue); });
}

// Null queues are still a programming error and surface as std::invalid_argument.
TEST(SerialOutput, CreateSerialOutput_InvalidOutputQueue) {
    auto logger = std::make_shared<NiceMockLogger>();
    auto inputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

    EXPECT_THROW(
        { auto serialOutput = SerialHandler(logger, "/dev/null", UARTDevice::A, nullptr, inputQueue); },
        std::invalid_argument);
}

TEST(SerialOutput, CreateSerialOutput_InvalidInputQueue) {
    auto logger = std::make_shared<NiceMockLogger>();
    auto outputQueue = std::make_shared<MessageQueue<creatures::io::Message>>();

    EXPECT_THROW(
        { auto serialOutput = SerialHandler(logger, "/dev/null", UARTDevice::A, outputQueue, nullptr); },
        std::invalid_argument);
}

} // namespace creatures

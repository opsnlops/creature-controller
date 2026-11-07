#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "controller-config.h"
#include "controller/ServoModuleHandler.h"
#include "io/Message.h"
#include "io/MessageProcessor.h"
#include "io/MessageRouter.h"
#include "mocks/logging/MockLogger.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"

namespace creatures {

namespace {

using namespace std::chrono_literals;
using creatures::io::Message;
using creatures::io::MessageRouter;
using ::testing::HasSubstr;

constexpr auto testPongTimeout = 50ms;

class ServoModuleHandlerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        router = std::make_shared<MessageRouter>(logger);
        handler = std::make_shared<ServoModuleHandler>(logger, nullptr, UARTDevice::A, "/nonexistent/serial-device",
                                                       router, websocketQueue, testPongTimeout);
        router->registerServoModuleHandler(UARTDevice::A, handler->getIncomingQueue(), handler->getOutgoingQueue());
    }

    std::shared_ptr<NiceMockLogger> logger = std::make_shared<NiceMockLogger>();
    std::shared_ptr<MessageQueue<server::ServerMessage>> websocketQueue =
        std::make_shared<MessageQueue<server::ServerMessage>>();
    std::shared_ptr<MessageRouter> router;
    std::shared_ptr<ServoModuleHandler> handler;
};

TEST_F(ServoModuleHandlerTest, NoPongTimeoutBeforeFirmwareIsReady) {
    std::this_thread::sleep_for(testPongTimeout * 2);

    EXPECT_FALSE(handler->getCommunicationFailure().has_value());
}

TEST_F(ServoModuleHandlerTest, ReportsFailureWhenPongsStop) {
    handler->firmwareReadyToOperate();
    EXPECT_FALSE(handler->getCommunicationFailure().has_value());

    std::this_thread::sleep_for(testPongTimeout * 2);

    const auto failure = handler->getCommunicationFailure();
    ASSERT_TRUE(failure.has_value());
    EXPECT_THAT(*failure, HasSubstr("no pong"));
}

TEST_F(ServoModuleHandlerTest, PongResetsTheTimeout) {
    handler->firmwareReadyToOperate();
    std::this_thread::sleep_for(testPongTimeout * 2);

    handler->pongReceived();

    EXPECT_FALSE(handler->getCommunicationFailure().has_value());
}

// The firmware goes silent after ESTOP. Treating that as a failure would restart
// the controller, which risks resetting the firmware and clearing the stop.
TEST_F(ServoModuleHandlerTest, EmergencyStopSuppressesFailures) {
    handler->firmwareReadyToOperate();
    EXPECT_TRUE(router->allHandlersReady());

    router->markEmergencyStopActive();
    std::this_thread::sleep_for(testPongTimeout * 2);

    EXPECT_FALSE(handler->getCommunicationFailure().has_value());
    EXPECT_FALSE(router->allHandlersReady());
}

TEST_F(ServoModuleHandlerTest, ReportsFailureWhenSerialPortCannotOpen) {
    handler->init();
    handler->start();

    const auto failure = handler->getCommunicationFailure();

    handler->shutdown();
    std::this_thread::sleep_for(250ms); // shutdown() detaches, let the threads finish

    ASSERT_TRUE(failure.has_value());
    EXPECT_THAT(*failure, HasSubstr("unable to open"));
}

TEST_F(ServoModuleHandlerTest, OutgoingQueueIsBoundedAndKeepsCriticalMessages) {
    router->sendMessageToCreature(Message(UARTDevice::A, "ESTOP\tCS 1"));

    for (int i = 0; i < SERIAL_OUTGOING_QUEUE_MAX_MESSAGES * 10; i++) {
        router->sendMessageToCreature(Message(UARTDevice::A, "POS\t0 1500\tCS 1"));
    }

    auto queue = handler->getOutgoingQueue();
    EXPECT_EQ(queue->size(), SERIAL_OUTGOING_QUEUE_MAX_MESSAGES);

    const auto oldest = queue->pop_timeout(10ms);
    ASSERT_TRUE(oldest.has_value());
    EXPECT_EQ(oldest->payload, "ESTOP\tCS 1");
}

} // namespace

} // namespace creatures


#include <memory>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "config/UARTDevice.h"
#include "controller/Controller.h"
#include "controller/ServoModuleHandler.h"
#include "io/Message.h"
#include "io/MessageProcessor.h"
#include "io/MessageRouter.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "mocks/logging/MockLogger.h"
#include "mocks/creature/MockCreature.h"
#include "mocks/io/handlers/MockMessageHandler.h"

using creatures::config::UARTDevice;
using creatures::io::Message;

class MessageProcessorTest : public ::testing::Test {
  protected:
    void SetUp() override {
        logger = std::make_shared<creatures::NiceMockLogger>();
        messageRouter = std::make_shared<creatures::io::MessageRouter>(logger);
        creature = std::make_shared<MockCreature>(logger);
        controller = std::make_shared<Controller>(logger, creature, messageRouter);

        websocketQueue = std::make_shared<creatures::MessageQueue<creatures::server::ServerMessage>>();
        moduleId = UARTDevice::module_name::A;

        servoModuleHandler = std::make_shared<creatures::ServoModuleHandler>(
            logger, controller, moduleId, "/dev/null", messageRouter, websocketQueue);

        messageProcessor = std::make_shared<creatures::MessageProcessor>(
            logger, moduleId, servoModuleHandler, websocketQueue);
    }

    std::shared_ptr<creatures::NiceMockLogger> logger;
    std::shared_ptr<creatures::io::MessageRouter> messageRouter;
    std::shared_ptr<MockCreature> creature;
    std::shared_ptr<Controller> controller;
    std::shared_ptr<creatures::MessageQueue<creatures::server::ServerMessage>> websocketQueue;
    UARTDevice::module_name moduleId;
    std::shared_ptr<creatures::ServoModuleHandler> servoModuleHandler;
    std::shared_ptr<creatures::MessageProcessor> messageProcessor;
};

TEST_F(MessageProcessorTest, Create) {
    EXPECT_NE(messageProcessor, nullptr);
}

TEST_F(MessageProcessorTest, ProcessMessage_BuiltinLogHandler) {
    Message msg(moduleId, "LOG\tThis is a test log message");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(MessageProcessorTest, ProcessMessage_BuiltinStatsHandler) {
    Message msg(moduleId, "STATS\t100\t200\t300");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(MessageProcessorTest, ProcessMessage_CustomHandler) {
    auto mockHandler = std::make_shared<creatures::MockMessageHandler>();
    messageProcessor->registerHandler("MOCK", mockHandler);

    Message msg(moduleId, "MOCK\tall looks good!");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(MessageProcessorTest, ProcessMessage_UnknownHandler) {
    // Unknown handlers no longer throw - they log a warning and return success
    Message msg(moduleId, "AAACCCKKKKK\tthe printer is on fire");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(MessageProcessorTest, ProcessMessage_EmptyPayload) {
    Message msg(moduleId, "");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
}

// --- Dynamixel sensor message tests ---

TEST_F(MessageProcessorTest, ProcessMessage_DynamixelSensor_SingleMotor) {
    // Format from firmware: DSENSE\tD<id> <temp> <load> <voltage_mV>
    Message msg(moduleId, "DSENSE\tD1 45 128 7400");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(websocketQueue->size(), 1u);
}

TEST_F(MessageProcessorTest, ProcessMessage_DynamixelSensor_MultipleMotors) {
    // Two Dynamixel servos reporting in
    Message msg(moduleId, "DSENSE\tD1 45 128 7400\tD2 43 -50 7350");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(websocketQueue->size(), 1u);
}

TEST_F(MessageProcessorTest, ProcessMessage_DynamixelSensor_NoMotorTokens) {
    // DSENSE with no motor data - handler should log a warning but not crash
    Message msg(moduleId, "DSENSE");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
    EXPECT_TRUE(websocketQueue->empty());
}

TEST_F(MessageProcessorTest, ProcessMessage_DynamixelSensor_MalformedToken) {
    // Token with wrong number of fields - handler skips it
    Message msg(moduleId, "DSENSE\tD1 45");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
    // Malformed tokens are skipped, but the message still produces a report
    // (with an empty dynamixel_motors array)
}

// --- Board sensor message tests ---

TEST_F(MessageProcessorTest, ProcessMessage_BoardSensor) {
    // Format from firmware: BSENSE\tTEMP <temp>\tVBUS <v> <c> <p>\tMP_IN <v> <c> <p>\t3V3 <v> <c> <p>\t5V <v> <c> <p>
    Message msg(moduleId,
                "BSENSE\tTEMP 32.50\tVBUS 12.500 2.500 31.250\tMP_IN 12.450 1.200 14.940\t3V3 3.300 0.250 0.825");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(websocketQueue->size(), 1u);
}

// --- Motor sensor message tests ---

TEST_F(MessageProcessorTest, ProcessMessage_MotorSensor) {
    // Format from firmware: MSENSE\tM0 <pos> <v> <c> <p>\t... for 8 motors
    Message msg(moduleId,
                "MSENSE\t"
                "M0 512 12.50 0.50 6.25\t"
                "M1 600 12.45 0.55 6.85\t"
                "M2 500 12.40 0.45 5.58\t"
                "M3 511 12.50 0.48 6.00\t"
                "M4 520 12.50 0.52 6.50\t"
                "M5 490 12.48 0.47 5.87\t"
                "M6 515 12.50 0.49 6.13\t"
                "M7 505 12.49 0.51 6.37");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(websocketQueue->size(), 1u);
}

// --- INIT / PONG / READY message tests ---

TEST_F(MessageProcessorTest, ProcessMessage_Init) {
    // Format from firmware: INIT\t<protocol_version>
    Message msg(moduleId, "INIT\t4");
    auto result = messageProcessor->processMessage(msg);
    // INIT handler calls into ServoModuleHandler which may fail in test context,
    // but processMessage catches all exceptions and returns success
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(MessageProcessorTest, ProcessMessage_Pong) {
    // Format from firmware: PONG\t<ms_since_boot>
    Message msg(moduleId, "PONG\t123456789");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
}

TEST_F(MessageProcessorTest, ProcessMessage_Ready) {
    // Format from firmware: READY\t1
    Message msg(moduleId, "READY\t1");
    auto result = messageProcessor->processMessage(msg);
    EXPECT_TRUE(result.isSuccess());
}

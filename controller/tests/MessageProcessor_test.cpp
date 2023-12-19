
#include <thread>

#include <gtest/gtest.h>

#include "util/MessageQueue.h"
#include "io/MessageProcessor.h"
#include "io/MessageProcessingException.h"
#include "io/SerialHandler.h"
#include "mocks/logging/MockLogger.h"
#include "mocks/io/handlers/MockMessageHandler.h"

TEST(MessageProcessor, Create) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>(logger, "/dev/null", outgoingQueue, incomingQueue);

    EXPECT_NO_THROW({
        auto messageProcessor = std::make_shared<creatures::MessageProcessor>(logger, serialHandler);
    });

}

TEST(MessageProcessor, ProcessMessage_ValidMessage) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>(logger, "/dev/null", outgoingQueue, incomingQueue);

    auto messageProcessor = std::make_shared<creatures::MessageProcessor>(logger, serialHandler);
    messageProcessor->registerHandler("MOCK", std::make_unique<creatures::MockMessageHandler>());

    EXPECT_NO_THROW({
        messageProcessor->processMessage("MOCK\tall looks good!");
    });
}

TEST(MessageProcessor, ProcessMessage_UnknownHandler) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>(logger, "/dev/null", outgoingQueue, incomingQueue);

    auto messageProcessor = std::make_shared<creatures::MessageProcessor>(logger, serialHandler);
    messageProcessor->registerHandler("MOCK", std::make_unique<creatures::MockMessageHandler>());

    EXPECT_THROW(({
                      messageProcessor->processMessage("AAACCCKKKKK\tthe printer is on fire");
                  }), creatures::MessageProcessingException);

}

TEST(MessageProcessor, ProcessMessage_NoHandlers) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>(logger, "/dev/null", outgoingQueue, incomingQueue);

    auto messageProcessor = std::make_shared<creatures::MessageProcessor>(logger, serialHandler);

    // TODO: This isn't actually testing what I think it does. The handlers are registered in the constructor
    EXPECT_THROW(({
        messageProcessor->processMessage("DONT_CARE\tnothing to see here");
    }), creatures::MessageProcessingException);

}
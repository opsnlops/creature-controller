
#include <thread>

#include <gtest/gtest.h>

#include "util/MessageQueue.h"
#include "io/MessageProcessor.h"
#include "io/SerialHandler.h"

TEST(MessageProcessor, Create) {

    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>("/dev/null", outgoingQueue, incomingQueue);

    EXPECT_NO_THROW({
        auto messageProcessor = std::make_shared<creatures::MessageProcessor>(serialHandler);
        messageProcessor->start();
    });

}

TEST(MessageProcessor, LogMessage) {

    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>("/dev/null", outgoingQueue, incomingQueue);

    auto messageProcessor = std::make_shared<creatures::MessageProcessor>(serialHandler);
    messageProcessor->start();

    // This isn't very good
    EXPECT_NO_THROW({
        incomingQueue->push("LOG asdfasdfasdf");
    });
}
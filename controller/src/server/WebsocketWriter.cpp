
#include <string>
#include <thread>

#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "server/WebsocketWriter.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"
#include "util/thread_name.h"

#include "controller-config.h"

namespace creatures::server {

WebsocketWriter::WebsocketWriter(const std::shared_ptr<Logger> &logger, std::shared_ptr<ix::WebSocket> webSocket,
                                 const std::shared_ptr<MessageQueue<ServerMessage>> &outgoingQueue,
                                 std::string creatureId, bool enabled) {
    this->logger = logger;
    this->webSocket = webSocket;
    this->outgoingQueue = outgoingQueue;
    this->creatureId = creatureId;
    this->enabled = enabled;

    this->logger->info("WebsocketWriter created");
}

void WebsocketWriter::start() {
    this->logger->info("starting the websocketwriter thread");
    creatures::StoppableThread::start();
}

void WebsocketWriter::run() {

    this->threadName = fmt::format("WebsocketWriter::run");
    setThreadName(threadName);

    this->logger->info("hello from the WebsocketWriter thread!");

    while (!stop_requested.load()) {
        auto outgoingMessage = outgoingQueue->pop_timeout(std::chrono::milliseconds(100));

        if (!outgoingMessage.has_value()) {
            continue;
        }

        // If we're not enabled, just continue and don't send the message. We
        // need to chew things off the queue so that we don't leak memory 😅
        if (!this->enabled) {
            logger->debug("skipping message because we're not enabled");
            continue;
        }

        auto messageResult = outgoingMessage.value().toWebSocketMessage(creatureId);
        if (!messageResult.isSuccess()) {
            this->logger->error("failed to convert message to websocket message");
            continue;
        }
        auto message = messageResult.getValue().value();

        this->logger->debug("message to write to websocket: {}", message);
        webSocket->sendUtf8Text(message);
        logger->debug("sent message to websocket");
    }

    logger->info("WebsocketWriter thread stopping");
}

} // namespace creatures::server
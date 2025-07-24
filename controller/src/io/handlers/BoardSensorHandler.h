
#pragma once

#include "io/handlers/IMessageHandler.h"
#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"

namespace creatures {

class BoardSensorHandler : public IMessageHandler {
  public:
    BoardSensorHandler(std::shared_ptr<Logger> logger,
                       std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue);
    void handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) override;

  private:
    std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue;
    std::shared_ptr<Logger> logger;
};

} // namespace creatures

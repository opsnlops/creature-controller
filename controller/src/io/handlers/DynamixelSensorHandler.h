
#pragma once

#include "creature/logging/Logger.h"
#include "io/handlers/IMessageHandler.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"

namespace creatures {

class DynamixelSensorHandler : public IMessageHandler {
  public:
    DynamixelSensorHandler(std::shared_ptr<Logger> logger,
                           std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue);
    void handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) override;

  private:
    std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue;
    std::shared_ptr<Logger> logger;
};

} // namespace creatures

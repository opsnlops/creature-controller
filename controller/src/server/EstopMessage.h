#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "server/ServerMessage.h"

namespace creatures::server {

class EstopMessage : public ServerMessage {
  public:
    EstopMessage(std::shared_ptr<Logger> logger, const json &message) {
        this->logger = logger;
        this->commandType = "emergency-stop";
        this->message = message;
    }
};

} // namespace creatures::server
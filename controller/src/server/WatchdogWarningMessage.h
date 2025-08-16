#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "server/ServerMessage.h"

namespace creatures::server {

class WatchdogWarningMessage : public ServerMessage {
  public:
    WatchdogWarningMessage(std::shared_ptr<Logger> logger, const json &message) {
        this->logger = logger;
        this->commandType = "watchdog-warning";
        this->message = message;
    }
};

} // namespace creatures::server
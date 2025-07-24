
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "server/ServerMessage.h"

namespace creatures ::server {

class BoardSensorReportMessage : public ServerMessage {
  public:
    BoardSensorReportMessage(std::shared_ptr<Logger> logger,
                             const json &message) {
        this->logger = logger;
        this->commandType = "board-sensor-report";
        this->message = message;
    }
};

} // namespace creatures::server

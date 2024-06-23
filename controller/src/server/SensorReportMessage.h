
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "server/ServerMessage.h"

namespace creatures :: server {

    class SensorReportMessage : public ServerMessage {
    public:
        SensorReportMessage(std::shared_ptr<Logger> logger, const json &message) {
            this->logger = logger;
            this->commandType = "sensor-report";
            this->message = message;
        }
    };

} // creatures :: server

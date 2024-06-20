
#pragma once

#include "logging/Logger.h"
#include "io/handlers/IMessageHandler.h"


#define SENSOR_MESSAGE      "SENSOR"


namespace creatures {

    class StatsHandler : public IMessageHandler {
    public:
        void handle(std::shared_ptr<Logger> logger, const std::vector<std::string>& tokens) override;
    };

} // creatures

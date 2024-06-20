
#pragma once

#include "logging/Logger.h"
#include "io/handlers/IMessageHandler.h"

namespace creatures {

    class SensorHandler : public IMessageHandler {
    public:
        void handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) override;
    };

} // creatures

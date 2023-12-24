
#pragma once

#include "logging/Logger.h"
#include "io/handlers/IMessageHandler.h"


namespace creatures {

    class PongHandler : public IMessageHandler {
    public:
        void handle(std::shared_ptr<Logger> logger, const std::vector<std::string>& tokens) override;
    };

} // creatures

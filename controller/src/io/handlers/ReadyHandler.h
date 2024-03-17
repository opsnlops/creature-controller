
#pragma once

#include "controller/ServoModuleHandler.h"
#include "logging/Logger.h"
#include "io/handlers/IMessageHandler.h"

namespace creatures {

    class ServoModuleHandler;

    class ReadyHandler : public IMessageHandler {

    public:
        ReadyHandler(std::shared_ptr<Logger> logger, std::shared_ptr<ServoModuleHandler> servoModuleHandler);
        void handle(std::shared_ptr<Logger> logger, const std::vector<std::string>& tokens) override;

    private:
        std::shared_ptr<ServoModuleHandler> servoModuleHandler;
    };

} // creatures



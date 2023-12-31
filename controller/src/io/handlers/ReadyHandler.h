
#pragma once

#include "controller/Controller.h"
#include "logging/Logger.h"
#include "io/handlers/IMessageHandler.h"

namespace creatures {

    class ReadyHandler : public IMessageHandler {
    public:
        ReadyHandler(std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller);
        void handle(std::shared_ptr<Logger> logger, const std::vector<std::string>& tokens) override;

    private:
        std::shared_ptr<Controller> controller;
    };

} // creatures



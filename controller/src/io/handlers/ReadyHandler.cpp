
#include <chrono>
#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "io/handlers/ReadyHandler.h"


namespace creatures {

    ReadyHandler::ReadyHandler(std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller) :
            controller(controller) {

        logger->info("ReadyHandler created!");
    }

    void ReadyHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {

        // This is basically the most easy handler ever 😅

        logger->info("READY message received from the firmware!");
        controller->firmwareReadyToOperate();

    }

} // creatures

#include <string>
#include <vector>

#include "controller/ServoModuleHandler.h"
#include "logging/Logger.h"
#include "io/handlers/ReadyHandler.h"


namespace creatures {

    using creatures::ServoModuleHandler;

    ReadyHandler::ReadyHandler(std::shared_ptr<Logger> logger, std::shared_ptr<ServoModuleHandler> servoModuleHandler) :
            servoModuleHandler(servoModuleHandler) {
        logger->info("ReadyHandler created!");
    }

    void ReadyHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {

        // This is basically the most easy handler ever 😅

        (void)tokens; // Unused

        logger->info("READY message received from the firmware!");
        servoModuleHandler->firmwareReadyToOperate();
    }

} // creatures
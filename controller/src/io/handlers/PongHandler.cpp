
#include <chrono>
#include <string>
#include <vector>

#include "controller/ServoModuleHandler.h"
#include "logging/Logger.h"
#include "io/handlers/PongHandler.h"

extern std::chrono::time_point<std::chrono::high_resolution_clock> lastPingSentAt;

namespace creatures {


    PongHandler::PongHandler(std::shared_ptr<Logger> logger, std::shared_ptr<ServoModuleHandler> servoModuleHandler) :
            servoModuleHandler(servoModuleHandler) {

        logger->info("PongHandler created for module {}!",
                     UARTDevice::moduleNameToString(servoModuleHandler->getModuleName()));
    }

    void PongHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {

        (void)tokens; // Unused

        // When did we receive this?
        auto pongTime = std::chrono::high_resolution_clock::now();

        // How long did it take?
        auto pingTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(pongTime - lastPingSentAt).count();

        auto pongMessage = fmt::format("pong from firmware for module {}! ({}us)",
                                       UARTDevice::moduleNameToString(servoModuleHandler->getModuleName()),
                                       pingTimeMicroseconds);
        logger->info(pongMessage);
        servoModuleHandler->sendMessageToController(pongMessage);
    }

} // creatures
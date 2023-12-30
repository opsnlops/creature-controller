
#include <chrono>
#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "io/handlers/InitHandler.h"
#include "util/string_utils.h"




#include "io/MessageProcessingException.h"


namespace creatures {

    InitHandler::InitHandler(std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller) :
    controller(controller) {

        logger->info("InitHandler created!");
    }


    void InitHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {

        if(tokens.size() != 2) {
            std::string errorMessage = fmt::format("Not enough tokens in the InitHandler! Expected 2, got {}", tokens.size());
            logger->error(errorMessage);
            throw MessageProcessingException(errorMessage);
        }

        u32 firmwareVersion = stringToU32(tokens[1]);
        logger->info("Firmware checked in and wants it's configuration! Version: {}", firmwareVersion);

        // Let the controller know it's time to party
        controller->firmwareReadyForInitialization(firmwareVersion);

    }

} // creatures
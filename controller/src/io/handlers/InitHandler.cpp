
#include <string>
#include <vector>

#include "io/handlers/InitHandler.h"

#include "controller/ServoModuleHandler.h"
#include "logging/Logger.h"

#include "util/string_utils.h"


#include "io/MessageProcessingException.h"


namespace creatures {

    InitHandler::InitHandler(std::shared_ptr<Logger> logger, std::shared_ptr<ServoModuleHandler> servoModuleHandler) :
            servoModuleHandler(servoModuleHandler) {

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
#warning fix
        //servoModuleHandler->firmwareReadyForInitialization(firmwareVersion);

    }

} // creatures


#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "io/handlers/StatsHandler.h"

namespace creatures {

    void StatsHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {

        logger->debug("incoming stats!");
        for(std::string token : tokens) {
            logger->debug(" {}", token);
        }
    }

} // creatures
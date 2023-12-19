

#include <string>
#include <vector>

#include "controller-config.h"
#include "logging/Logger.h"

#include "io/handlers/LogHandler.h"

#define FIRMWARE_LOGGING_VERBOSE "[V]"
#define FIRMWARE_LOGGING_DEBUG   "[D]"
#define FIRMWARE_LOGGING_INFO    "[I]"
#define FIRMWARE_LOGGING_WARNING "[W]"
#define FIRMWARE_LOGGING_ERROR   "[E]"
#define FIRMWARE_LOGGING_FATAL   "[F]"
#define FIRMWARE_LOGGING_UNKNOWN "[?]"

namespace creatures {

    void LogHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {
         // 0       1       2       3
        // LOG \t time \t level \t message

#if DEBUG_MESSAGE_PROCESSING
        logger->trace("incoming log message");
        for(std::string token : tokens) {
            trace(" {}", token);
        }
#endif
        auto level = tokens[2];
        auto message = tokens[3];

        if (level == FIRMWARE_LOGGING_VERBOSE) logger->trace("{}", message);
        else if (level == FIRMWARE_LOGGING_DEBUG) logger->debug("{}", message);
        else if (level == FIRMWARE_LOGGING_INFO) logger->info("{}", message);
        else if (level == FIRMWARE_LOGGING_WARNING) logger->warn("{}", message);
        else if (level == FIRMWARE_LOGGING_ERROR) logger->error("{}", message);
        else if (level == FIRMWARE_LOGGING_FATAL) logger->critical("{}", message);
        else logger->warn("Unknown logging level from firmware: {}, message: {}", level, message);
    }

} // creatures


#include <string>
#include <vector>

#include "controller-config.h"
#include "namespace-stuffs.h"

#include "io/handlers/LogHandler.h"

#define FIRMWARE_LOGGING_VERBOSE "[V]"
#define FIRMWARE_LOGGING_DEBUG   "[D]"
#define FIRMWARE_LOGGING_INFO    "[I]"
#define FIRMWARE_LOGGING_WARNING "[W]"
#define FIRMWARE_LOGGING_ERROR   "[E]"
#define FIRMWARE_LOGGING_FATAL   "[F]"
#define FIRMWARE_LOGGING_UNKNOWN "[?]"

namespace creatures {

    void LogHandler::handle(const std::vector<std::string> &tokens) {
         // 0       1       2       3
        // LOG \t time \t level \t message

#if DEBUG_MESSAGE_PROCESSING
        trace("incoming log message");
        for(std::string token : tokens) {
            trace(" {}", token);
        }
#endif
        auto level = tokens[2];
        auto message = tokens[3];

        if (level == FIRMWARE_LOGGING_VERBOSE) trace("{}", message);
        else if (level == FIRMWARE_LOGGING_DEBUG) debug("{}", message);
        else if (level == FIRMWARE_LOGGING_INFO) info("{}", message);
        else if (level == FIRMWARE_LOGGING_WARNING) warn("{}", message);
        else if (level == FIRMWARE_LOGGING_ERROR) error("{}", message);
        else if (level == FIRMWARE_LOGGING_FATAL) critical("{}", message);
        else warn("Unknown logging level from firmware: {}, message: {}", level, message);
    }

} // creatures
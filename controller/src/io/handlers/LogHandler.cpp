

#include <string>
#include <vector>

#include "controller-config.h"
#include "logging/Logger.h"

#include "io/handlers/LogHandler.h"

#define FIRMWARE_LOGGING_VERBOSE "[V]"
#define FIRMWARE_LOGGING_DEBUG "[D]"
#define FIRMWARE_LOGGING_INFO "[I]"
#define FIRMWARE_LOGGING_WARNING "[W]"
#define FIRMWARE_LOGGING_ERROR "[E]"
#define FIRMWARE_LOGGING_FATAL "[F]"
#define FIRMWARE_LOGGING_UNKNOWN "[?]"

namespace creatures {

void LogHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {
    // 0       1       2       3
    // LOG \t time \t level \t message

#if DEBUG_MESSAGE_PROCESSING
    logger->trace("incoming log message");
    for (std::string token : tokens) {
        trace(" {}", token);
    }
#endif
    if (tokens.size() < 4) {
        logger->error("Invalid number of tokens in log message: {}", tokens.size());
        return;
    }

    // Carry the firmware's own ms-since-boot (tokens[1]) through to the output.
    // Our timestamp is when we got around to printing the line, which says
    // nothing about the order the firmware actually emitted things in — and
    // that ordering is exactly what you need when reading through an init or a
    // power-restore sequence.
    //
    // The firmware's text stays an argument rather than becoming part of the
    // format string; it is not ours and may legitimately contain braces.
    auto uptime = tokens[1];
    auto level = tokens[2];
    auto message = tokens[3];

    if (level == FIRMWARE_LOGGING_VERBOSE)
        logger->trace("📟 [{}ms] {}", uptime, message);
    else if (level == FIRMWARE_LOGGING_DEBUG)
        logger->debug("📟 [{}ms] {}", uptime, message);
    else if (level == FIRMWARE_LOGGING_INFO)
        logger->info("📟 [{}ms] {}", uptime, message);
    else if (level == FIRMWARE_LOGGING_WARNING)
        logger->warn("📟 [{}ms] {}", uptime, message);
    else if (level == FIRMWARE_LOGGING_ERROR)
        logger->error("📟 [{}ms] {}", uptime, message);
    else if (level == FIRMWARE_LOGGING_FATAL)
        logger->critical("📟 [{}ms] {}", uptime, message);
    else
        logger->warn("Unknown logging level from firmware: {}, message: {}", level, message);
}

} // namespace creatures
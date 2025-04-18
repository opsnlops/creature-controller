// Don't yell at me at the pass-by-value on the logger

#include <chrono>

#include <fmt/format.h>

#include "controller-config.h"


#include "controller/commands/tokens/ServoPosition.h"

#include "CommandException.h"
#include "Ping.h"



namespace creatures::commands {

    Ping::Ping(std::shared_ptr<Logger> logger) : logger(logger) {}

    std::string Ping::toMessage() {

        const auto now = std::chrono::high_resolution_clock::now();

        // Start the message with the 'PING' command prefix
        std::string message = fmt::format("{}\t{}", "PING",
                                          std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());

        logger->trace("message is: {}", message);
        return message;
    }

} // creatures::commands


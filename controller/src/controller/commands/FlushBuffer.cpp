


#include "controller-config.h"


#include "CommandException.h"
#include "FlushBuffer.h"



namespace creatures::commands {

    FlushBuffer::FlushBuffer(const std::shared_ptr<Logger> &logger) : logger(logger) {}

    std::string FlushBuffer::toMessage() {

        // Only one character, the bell! 🔔
        std::string message = "\a";

        logger->debug("constructed a FlushBuffer message 🔔");
        return message;
    }

} // creatures::commands

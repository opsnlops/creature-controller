
#pragma once

#include "controller-config.h"

#include "logging/Logger.h"
#include "controller/commands/ICommand.h"


namespace creatures::commands {

    /**
     * This is a very simple simple message that contains only a single character. It's
     * a magic signal to the firmware to flush its buffer and get ready to start over.
     * That magic character is `\a`, a bell character. 🔔
     */
    class FlushBuffer : public ICommand {

    public:
        FlushBuffer(std::shared_ptr<Logger> logger);

        std::string toMessage() override;

    private:

        std::shared_ptr<Logger> logger;
    };

} // creatures::commands


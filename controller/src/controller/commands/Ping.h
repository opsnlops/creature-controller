
#pragma once

#include "controller-config.h"

#include "logging/Logger.h"
#include "controller/commands/ICommand.h"


namespace creatures::commands {

    class Ping final : public ICommand {

    public:
        explicit Ping(std::shared_ptr<Logger> logger);

        std::string toMessage() override;

    private:

        std::shared_ptr<Logger> logger;
    };

} // creatures::commands


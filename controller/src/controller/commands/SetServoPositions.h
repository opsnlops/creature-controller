
#pragma once

#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "config/UARTDevice.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/tokens/ServoPosition.h"


namespace creatures::commands {

    class SetServoPositions : public ICommand {

    public:
        explicit SetServoPositions(std::shared_ptr<Logger> logger);
        void addServoPosition(const ServoPosition& servoPosition);

        void setFilter(creatures::config::UARTDevice::module_name _filter);

        std::string toMessage() override;

    private:

        std::vector<ServoPosition> servoPositions;
        std::shared_ptr<Logger> logger;
        creatures::config::UARTDevice::module_name filter;
    };

} // creatures::commands



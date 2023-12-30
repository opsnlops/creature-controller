
#pragma once

#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/tokens/ServoPosition.h"


namespace creatures::commands {

    class SetServoPositions : public ICommand {

    public:
        SetServoPositions(std::shared_ptr<Logger> logger);
        void addServoPosition(const ServoPosition& servoPosition);

        std::string toMessage() override;

    private:

        std::vector<ServoPosition> servoPositions;
        std::shared_ptr<Logger> logger;
    };

} // creatures::commands



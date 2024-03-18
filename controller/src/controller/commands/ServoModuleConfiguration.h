
#pragma once

#include <string>
#include <vector>

#include "controller-config.h"

#include "creature/Creature.h"
#include "device/Servo.h"
#include "logging/Logger.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/tokens/ServoConfig.h"

namespace creatures::commands {

    class ServoModuleConfiguration : public ICommand  {

    public:
        explicit ServoModuleConfiguration(std::shared_ptr<Logger> logger);

        /**
         * Go fetch the servo configurations from the creature
         */
        void getServoConfigurations(std::shared_ptr<creatures::creature::Creature> creature);

        std::string toMessage() override;

    private:
        std::vector<ServoConfig> servoConfigurations;
        std::shared_ptr<Logger> logger;

        void addServoConfig(const ServoConfig &servoConfig);

    };

} // creatures::commands



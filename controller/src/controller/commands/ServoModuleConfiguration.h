
#pragma once

#include <string>
#include <vector>

#include "controller-config.h"

#include "creature/Creature.h"
#include "device/Servo.h"
#include "logging/Logger.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/tokens/ServoConfig.h"
#include "util/Result.h"

namespace creatures::commands {

    class ServoModuleConfiguration : public ICommand  {

    public:
        explicit ServoModuleConfiguration(std::shared_ptr<Logger> logger);

        /**
         * Go fetch the servo configurations from the creature
         */
        Result<bool> getServoConfigurations(const std::shared_ptr<Controller>& controller,
                                            creatures::config::UARTDevice::module_name module);

        std::string toMessage() override;

    private:
        std::vector<ServoConfig> servoConfigurations;
        std::shared_ptr<Logger> logger;

        Result<bool> addServoConfig(const ServoConfig &servoConfig);

    };

} // creatures::commands



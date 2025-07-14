#pragma once

#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/tokens/ServoConfig.h"
#include "util/Result.h"

// Forward declarations to avoid circular includes
class Controller;

namespace creatures::commands {

    /**
     * Creates a configuration message to send to the firmware
     *
     * This command gathers servo configurations from the controller and formats
     * them into a message that the firmware can understand. Keep it simple and
     * reliable, like a well-organized rabbit burrow! 🐰
     */
    class ServoModuleConfiguration final : public ICommand  {

    public:
        explicit ServoModuleConfiguration(std::shared_ptr<Logger> logger);

        /**
         * Fetch servo configurations from the controller for a specific module
         *
         * @param controller the controller to get configurations from
         * @param module the module to get configurations for
         * @return Result indicating success or failure
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
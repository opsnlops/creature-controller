#pragma once

#include <string>

#include "device/Servo.h"
#include "logging/Logger.h"

#include "controller-config.h"

namespace creatures {

/**
 * @brief Represents a servo configuration for transmission to the firmware
 *
 * This class encapsulates the configuration parameters for a servo motor
 * that need to be sent to the firmware during initialization. It provides
 * a bridge between the controller's representation of a servo and the
 * configuration data needed by the firmware.
 */
class ServoConfig {

  public:
    /**
     * @brief Constructs a ServoConfig from a Servo object
     *
     * @param logger Shared pointer to the logging system
     * @param servo Shared pointer to the Servo being configured
     */
    ServoConfig(const std::shared_ptr<Logger> &logger,
                const std::shared_ptr<Servo> &servo);

    /**
     * @brief Default destructor
     */
    ~ServoConfig() = default;

    /**
     * @brief Gets the output header (pin) for this servo
     *
     * @return The GPIO pin number used for this servo
     */
    [[nodiscard]] u16 getOutputHeader() const;

    /**
     * @brief Converts the configuration to a string for transmission
     *
     * Creates a formatted string containing the servo configuration
     * parameters in the format expected by the firmware:
     * "SERVO [pin] [min_us] [max_us]"
     *
     * @return A formatted string representation of the configuration
     */
    [[nodiscard]] std::string toString() const;

  private:
    std::shared_ptr<Logger> logger; ///< Logger instance for debugging
    std::shared_ptr<Servo> servo;   ///< The servo being configured
};

} // namespace creatures
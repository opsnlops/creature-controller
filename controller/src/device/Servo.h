#pragma once

#include <string>

#include "controller-config.h"

#include "config/UARTDevice.h"
#include "device/ServoSpecifier.h"
#include "logging/Logger.h"
#include "util/Result.h"

/**
 * @brief A complete representation of a servo motor controlled by the system
 *
 * This class manages all aspects of a servo motor, including its physical properties,
 * identification, position limits, and current state. It provides methods to move the
 * servo, calculate positions, and retrieve servo information.
 *
 * The min and max pulse define the length of travel for this servo within
 * the creature. These values are highly specific to each individual creature's
 * physical construction and limitations.
 */
class Servo {

public:
    /**
     * @brief Constructs a new Servo instance
     *
     * @param logger Shared pointer to logger for status and error reporting
     * @param id Unique identifier string for this servo
     * @param name Human-readable name for this servo
     * @param outputLocation Hardware location (module and pin) of this servo
     * @param min_pulse_us Minimum pulse width in microseconds (lower position limit)
     * @param max_pulse_us Maximum pulse width in microseconds (upper position limit)
     * @param smoothingValue Factor (0.0-1.0) for movement smoothing - higher values mean slower, smoother movement
     * @param inverted Whether servo direction is inverted (true) or normal (false)
     * @param servo_update_frequency_hz Update frequency in Hz (typically 50Hz for standard servos)
     * @param default_position_microseconds Default position in microseconds to use on startup
     */
    Servo(std::shared_ptr<creatures::Logger> logger, std::string id, std::string name, ServoSpecifier outputLocation,
          u16 min_pulse_us, u16 max_pulse_us, float smoothingValue, bool inverted, u16 servo_update_frequency_hz,
          u16 default_position_microseconds);

    /**
     * @brief Enables the servo's PWM output
     */
    void turnOn();

    /**
     * @brief Disables the servo's PWM output
     */
    void turnOff();

    /**
     * @brief Gets the unique identifier of this servo
     * @return Servo ID string
     */
    [[nodiscard]] std::string getId() const;

    /**
     * @brief Gets the hardware location of this servo
     * @return ServoSpecifier containing module and pin information
     */
    [[nodiscard]] ServoSpecifier getOutputLocation() const;

    /**
     * @brief Gets the current position of the servo
     * @return Current position value between MIN_POSITION and MAX_POSITION
     */
    [[nodiscard]] u16 getPosition() const;

    /**
     * @brief Gets the default position in microseconds
     * @return Default position in microseconds
     */
    [[nodiscard]] u16 getDefaultMicroseconds() const;

    /**
     * @brief Gets the target position in microseconds
     * @return Target position in microseconds
     */
    [[nodiscard]] u32 getDesiredMicroseconds() const;

    /**
     * @brief Gets the current position in microseconds
     * @return Current position in microseconds
     */
    [[nodiscard]] u32 getCurrentMicroseconds() const;

    /**
     * @brief Gets the smoothing factor applied to movements
     * @return Smoothing factor (0.0-1.0)
     */
    [[nodiscard]] float getSmoothingValue() const;

    /**
     * @brief Gets the human-readable name of this servo
     * @return Servo name
     */
    [[nodiscard]] std::string getName() const;

    /**
     * @brief Requests the servo to move to a specific position
     *
     * This method validates the requested position is within bounds
     * and updates the desired_microseconds value. The actual movement
     * is performed by the PWM system during the next update cycle.
     *
     * @param position Position value between MIN_POSITION and MAX_POSITION
     * @return Result object containing success or failure information
     */
    creatures::Result<std::string> move(u16 position);

    /**
     * @brief Calculates the next position step based on smoothing
     *
     * This method implements motion smoothing by interpolating between
     * the current position and the desired position based on the smoothing factor.
     */
    void calculateNextTick();

    /**
     * @brief Checks if the servo direction is inverted
     * @return true if inverted, false if normal
     */
    [[nodiscard]] bool isInverted() const;

    /**
     * @brief Gets the module this servo is connected to
     * @return Module name enumeration
     */
    [[nodiscard]] creatures::config::UARTDevice::module_name getOutputModule() const;

    /**
     * @brief Gets the pin number this servo is connected to
     * @return Pin number
     */
    [[nodiscard]] u16 getOutputHeader() const;

    /**
     * @brief Gets the minimum pulse width in microseconds
     * @return Minimum pulse width
     */
    [[nodiscard]] u16 getMinPulseUs() const;

    /**
     * @brief Gets the maximum pulse width in microseconds
     * @return Maximum pulse width
     */
    [[nodiscard]] u16 getMaxPulseUs() const;

    /**
     * @brief Gets the servo update frequency in Hz
     * @return Update frequency
     */
    [[nodiscard]] u16 getServoUpdateFrequencyHz() const;

    /**
     * @brief Gets the length of one control frame in microseconds
     * @return Frame length in microseconds
     */
    [[nodiscard]] u32 getFrameLengthMicroseconds() const;

private:
    std::string id;                   ///< Unique identifier for this servo
    ServoSpecifier outputLocation;    ///< Hardware location (module and pin)
    u16 min_pulse_us;                 ///< Lower bound pulse size in microseconds
    u16 max_pulse_us;                 ///< Upper bound pulse size in microseconds
    u16 servo_update_frequency_hz;    ///< Update frequency in Hz (typically 50Hz)
    u32 frame_length_microseconds;    ///< Calculated from servo_update_frequency_hz
    u16 current_position;             ///< Current position in position units
    u16 default_microseconds;         ///< Default position in microseconds
    bool on;                          ///< Whether servo is enabled
    bool inverted;                    ///< Whether direction is inverted
    u32 desired_microseconds;         ///< Target position in microseconds
    u32 current_microseconds;         ///< Current position in microseconds
    std::string name;                 ///< Human-readable name
    float smoothingValue;             ///< Movement smoothing factor (0.0-1.0)
    std::shared_ptr<creatures::Logger> logger; ///< Logger instance

    /**
     * @brief Converts position value to microseconds
     *
     * "Position" is the device-independent value from input handlers.
     * This function maps it to device-specific microseconds.
     *
     * @param position Value between MIN_POSITION and MAX_POSITION
     * @return Position mapped to microseconds specific to this servo
     */
    u32 positionToMicroseconds(u16 position);

    /**
     * @brief Converts microseconds to position value
     *
     * Inverse of positionToMicroseconds().
     *
     * @param microseconds The pulse width in microseconds
     * @return Equivalent position value
     */
    u16 microsecondsToPosition(u32 microseconds);
};
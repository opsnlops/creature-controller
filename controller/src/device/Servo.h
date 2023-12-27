
#pragma once

#include <cstdio>
#include <string>


#include "controller-config.h"

#include "logging/Logger.h"

/**
 * A representation of a servo that we are controlling with the controller
 *
 * The min and max pulse define the length of the travel for this servo within
 * the creature. It is highly specific to the individual creature.
 *
 * @brief A representation of a servo motor
 */
class Servo {

public:
    Servo(std::shared_ptr<creatures::Logger> logger, std::string id, std::string outputLocation, std::string name, u16 min_pulse_us, u16 max_pulse_us,
          float smoothingValue, bool inverted, u16 servo_update_frequency_hz, u16 default_position);

    void turnOn();
    void turnOff();
    std::string getId() const;
    [[nodiscard]] u16 getPosition() const;
    [[nodiscard]] u16 getDefaultPosition() const;

    // These are PWM values
    [[nodiscard]] u32 getDesiredMicroseconds() const;   // Where we want it to go
    [[nodiscard]] u32 getCurrentMicroseconds() const;   // Where the servo currently is

    [[nodiscard]] float getSmoothingValue() const;
    [[nodiscard]] std::string getName() const;
    void move(u16 position);

    void calculateNextTick();

    [[nodiscard]] bool isInverted() const;
    [[nodiscard]] std::string getOutputLocation() const;
    [[nodiscard]] u16 getMinPulseUs() const;
    [[nodiscard]] u16 getMaxPulseUs() const;

    [[nodiscard]] u16 getServoUpdateFrequencyHz() const;
    [[nodiscard]] u32 getFrameLengthMicroseconds() const;



private:
    std::string id;               // This servo's id
    std::string outputLocation;     // Which servo board and pin is this servo on (ie, A0, A1, B0, etc.)
    u16 min_pulse_us;      // Lower bound on the servo's pulse size in microseconds
    u16 max_pulse_us;      // Upper bound on the servo's pulse size in microseconds
    u16 servo_update_frequency_hz;   // How fast should we tell the firmware to update the servo (usually 50Hz)
    u32 frame_length_microseconds;  // Calculated off the servo_update_frequency_hz
    u16 current_position;  // Where we think the servo currently is in our position
    u16 default_position;   // The position to go to when there's nothing else
    bool on;                    // Is the servo active?
    bool inverted;              // Should the movements be inverted?
    u32 desired_microseconds;     // The number of microseconds the servo should be set to next
    u32 current_microseconds;     // The current number of microseconds the servo is set to
    std::string name;           // This servo's name
    float smoothingValue;       // The constant to use when smoothing the input
    std::shared_ptr<creatures::Logger> logger;

};


#pragma once

#include <cstdio>
#include <string>


#include "controller-config.h"

#include "logging/Logger.h"

/**
 * A representation of a servo that we are controlling with the controller
 *
 * At init the slice, channel, and resolution are calculated from the Pico
 * SDK.
 *
 * The min and max pulse define the length of the travel for this servo within
 * the creature. It is highly specific to the individual creature.
 *
 * @brief A representation of a servo motor
 */
class Servo {

public:
    Servo(std::shared_ptr<creatures::Logger> logger, std::string id, u8 outputPin, std::string name, u16 min_pulse_us, u16 max_pulse_us,
          float smoothingValue, bool inverted, u16 default_position);
    void turnOn();
    void turnOff();
    std::string getId() const;
    [[nodiscard]] u16 getPosition() const;
    [[nodiscard]] u16 getDefaultPosition() const;
    [[nodiscard]] u8 getSlice() const;
    [[nodiscard]] u8 getChannel() const;

    // These are PWM values
    [[nodiscard]] u32 getDesiredTick() const;   // Where we want it to go
    [[nodiscard]] u32 getCurrentTick() const;   // Where the servo currently is

    [[nodiscard]] float getSmoothingValue() const;
    [[nodiscard]] std::string getName() const;
    void move(u16 position);

    void calculateNextTick();

    bool isInverted() const;
    u8 getOutputPin() const;
    u16 getMinPulseUs() const;
    u16 getMaxPulseUs() const;


private:
    std::string id;               // This servo's id
    u8 outputPin;                  // Pin on the board the servo is connected to
    u16 min_pulse_us;      // Lower bound on the servo's pulse size in microseconds
    u16 max_pulse_us;      // Upper bound on the servo's pulse size in microseconds
    u8 slice;                 // PWM slice for this servo (Pi Pico)
    u8 channel;               // PWM channel for this servo (Pi Pico)
    u32 resolution;        // The resolution for this servo
    u32 frame_length_us;   // How many microseconds are in each frame
    u16 current_position;  // Where we think the servo currently is in our position
    u16 default_position;   // The position to go to when there's nothing else
    bool on;                    // Is the servo active?
    bool inverted;              // Should the movements be inverted?
    u32 desired_ticks;     // The number of ticks we should be set to on the next cycle
    u32 current_ticks;     // Which tick is the servo at
    std::string name;           // This servo's name
    float smoothingValue;       // The constant to use when smoothing the input
    std::shared_ptr<creatures::Logger> logger;

};

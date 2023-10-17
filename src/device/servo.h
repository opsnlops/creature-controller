
#pragma once

#include <cstdio>
#include "controller-config.h"


/**
 * A representation of a servo that we are controlling with the controller
 *
 * At init the slice, channel, and resolution are calculated from the Pico
 * SDK.
 *
 * The min and max pulse define the length of the travel for this servo within
 * the creature. It is highly specific to the individual creature.
 *
 * BIG NOTE: Some of the GPIO pins share a slice. Be aware that the frequency must
 *           be the same for both halves of the slice.
 *
 * @brief A representation of a servo motor
 */
class Servo {

public:
    Servo(uint gpio, const char* name, u16 min_pulse_us, u16 max_pulse_us,
          float smoothingValue, bool inverted, u32 frequency);
    void turnOn();
    void turnOff();
    [[nodiscard]] u16 getPosition() const;
    [[nodiscard]] uint getSlice() const;
    [[nodiscard]] uint getChannel() const;

    // These are PWM values
    [[nodiscard]] u32 getDesiredTick() const;   // Where we want it to go
    [[nodiscard]] u32 getCurrentTick() const;   // Where the servo currently is

    [[nodiscard]] float getSmoothingValue() const;
    [[nodiscard]] const char* getName() const;
    void move(u16 position);

    void calculateNextTick();

private:
    u8 gpio;                  // GPIO pin the servo is connected to
    u16 min_pulse_us;      // Lower bound on the servo's pulse size in microseconds
    u16 max_pulse_us;      // Upper bound on the servo's pulse size in microseconds
    u8 slice;                 // PWM slice for this servo
    u8 channel;               // PWM channel for this servo
    u32 resolution;        // The resolution for this servo
    u32 frame_length_us;   // How many microseconds are in each frame
    u16 current_position;  // Where we think the servo currently is in our position
    bool on;                    // Is the servo active?
    bool inverted;              // Should the movements be inverted?
    u32 desired_ticks;     // The number of ticks we should be set to on the next cycle
    u32 current_ticks;     // Which tick is the servo at
    const char* name;           // This servo's name
    float smoothingValue;       // The constant to use when smoothing the input

    static u32 pwm_set_freq_duty(uint slice_num, uint chan, u32 frequency, int d);
};

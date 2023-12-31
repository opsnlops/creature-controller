
#include <cmath>
#include <utility>

#include "controller-config.h"

// This module
#include "Servo.h"

// Our modules
#include "ServoException.h"
#include "util/ranges.h"

extern u64 number_of_moves;

/**
 * @brief Initializes a servo and gets it ready for use
 *
 * The Servo that will be set up is the one that's passed in. Calling this function
 * will set up the PWM hardware to match the frequency that's provided. (Typically
 * 50Hz on a standard servo.)
 *
 * We work off pulse widths to servos, not percentages. The min and max pulse need
 * to be defined in microseconds. This will be mapped to the MIN_POSITION and
 * MAX_POSITION as defined in controller.h. (0-999 is typical.) The size of the
 * pulses need to be set to the particular servo and what it does inside of the
 * creature itself. (ie, don't bend a joint further than it should go!)
 *
 * The servo's PWM controller will be off by default to make sure it's not turned
 * on before we're ready to go.
 *
 * @param logger a shared pointer to our logger
 * @param id the id of this servo
 * @param outputLocation Which board and pin this servo is connected to (ie, A0, A1, B0, etc.)
 * @param min_pulse_us Min pulse length in microseconds
 * @param max_pulse_us Max pulse length in microseconds
 * @param smoothingValue how aggressive should our movement smoothing be
 * @param inverted are this servo's movements inverted?
 * @param servo_update_frequency_hz how fast should we tell the firmware to update the servos
 * @param default_position_microseconds the default position to set the servo to at start
 */
Servo::Servo(std::shared_ptr<creatures::Logger> logger, std::string id, std::string outputLocation, std::string name, u16 min_pulse_us,
             u16 max_pulse_us, float smoothingValue, bool inverted, u16 servo_update_frequency_hz, u16 default_position_microseconds) : logger(logger) {

    this->id = std::move(id);
    this->outputLocation = std::move(outputLocation);
    this->name = name;
    this->min_pulse_us = min_pulse_us;
    this->max_pulse_us = max_pulse_us;
    this->smoothingValue = smoothingValue;
    this->default_microseconds = default_position_microseconds;
    this->servo_update_frequency_hz = servo_update_frequency_hz;
    this->inverted = inverted;

    // Calculate the length of a frame in microseconds based on the frequency
    this->frame_length_microseconds = 1000000 / servo_update_frequency_hz;

    // Default to setting all of our values to the default that the config file
    // said we should use as our default
    this->desired_microseconds = inverted ? max_pulse_us - (getDefaultMicroseconds() - min_pulse_us) : getDefaultMicroseconds();
    this->current_microseconds = inverted ? max_pulse_us - (getDefaultMicroseconds() - min_pulse_us)  : getDefaultMicroseconds();
    this->current_position = inverted ? MAX_POSITION - microsecondsToPosition(getDefaultMicroseconds() - min_pulse_us) : microsecondsToPosition(getDefaultMicroseconds());

    // Force a calculation for the current tick
    calculateNextTick();

    // Turn the servo on by default
    this->on = false;

    logger->info("set up servo on location {}: name: {}, min_pulse: {}, max_pulse: {}, default: {}, inverted: {}",
                 outputLocation, name, min_pulse_us, max_pulse_us, default_position_microseconds, inverted ? "yes" : "no");
}

/**
 * @brief Turns on the PWM pulse for a servo
 *
 * Note that this turns on the PWM pulse _for the entire slice_ (both A and B
 * outputs).
 */
void Servo::turnOn() {
    //pwm_set_enabled(slice, true);
    on = true;

    logger->info("Enabled servo at location {}", outputLocation);
}


/**
 * @brief Turns off the PWM pulse for a servo
 *
 * Note that this turns off the PWM pulse _for the entire slice_ (both A and B
 * outputs).
 */
void Servo::turnOff() {
    //pwm_set_enabled(slice, false);
    on = false;

    logger->info("Disabled servo at location {}", outputLocation);
}

/**
 * @brief Requests that a servo be moved to a given position
 *
 * The value must be between `MIN_POSITION` and `MAX_POSITION`. An
 * assert will be fired in not to prevent damage to the creature or a motor.
 *
 * `servo_move()` does not actually move the servo. Instead, it marks it's requested
 * position in `desired_microseconds` and waits for the IRQ handler to fire off to actually
 * move to that position. Working this way decouples changing the duty cycle from
 * the request to the PWM circuit, which is useful when moving servos following
 * some sort of external control. (DMX in our case!)
 *
 * The Pi Pico only adjusts the duty cycle when the counter rolls over. Doing it
 * more often wastes CPU cycles. (Calling `pwm_set_chan_level()` more than once a
 * frame is a waste of effort.)
 *
 * The IRQ handler follows the frequency of the first servo in the array declared in
 * main(). I would like to be able to support a bunch of different frequencies at once,
 * but as of right now, there's really not a need, and I'd rather not make the ISR
 * any more complex than it needs to be.
 *
 * @param position The requested position
 */
void Servo::move(u16 position) {

    // Error checking. This could result in damage to a motor or
    // creature if not met, so this is a hard stop if it's wrong. 😱
    if(!(position >= MIN_POSITION && position <= MAX_POSITION)) {
        logger->critical("Servo::move() called with invalid position! min: {}, max: {}, requested: {}",
                 MIN_POSITION, MAX_POSITION, position);
        throw creatures::ServoException(fmt::format("Servo::move() called with invalid position! min: {}, max: {}, requested: {}",
                                                    MIN_POSITION, MAX_POSITION, position));
    };

    // If this servo is inverted, do it now
    if(inverted) position = MAX_POSITION - position;

    // Covert this to a desired microsecond
    desired_microseconds = positionToMicroseconds(position);

    // Save the position for debugging
    current_position = position;

    logger->trace("requesting servo on output location {} to be set to position {} ({}us)",
                  outputLocation,
                  current_position,
                  desired_microseconds);

    number_of_moves = number_of_moves + 1;
}

u32 Servo::positionToMicroseconds(u16 position) {
    return convertRange(logger, position, MIN_POSITION, MAX_POSITION, min_pulse_us, max_pulse_us);
}

u16 Servo::microsecondsToPosition(u32 microseconds) {
    return convertRange(logger, microseconds, min_pulse_us, max_pulse_us, MIN_POSITION, MAX_POSITION);
}

std::string Servo::getName() const {
    return name;
}

std::string Servo::getId() const {
    return id;
}

u16 Servo::getPosition() const {

    // If this is an inverted servo, show the inverted value
    if(inverted) {
        return MAX_POSITION - current_position;
    } else {
        return current_position;
    }
}

u16 Servo::getDefaultMicroseconds() const {
    return this->default_microseconds;
}

u32 Servo::getDesiredMicroseconds() const {
    return desired_microseconds;
}

u32 Servo::getCurrentMicroseconds() const {
    return current_microseconds;
}

float Servo::getSmoothingValue() const {
    return smoothingValue;
}

void Servo::calculateNextTick() {
    u32 last_tick = current_microseconds;

    current_microseconds = lround(((double)desired_microseconds * (1.0 - smoothingValue)) + ((double)last_tick * smoothingValue));
    //debug("-- set current_microseconds to {}", current_microseconds);
}

bool Servo::isInverted() const {
    return inverted;
};

std::string Servo::getOutputLocation() const {
    return outputLocation;
}

u16 Servo::getMinPulseUs() const {
    return min_pulse_us;
}

u16 Servo::getMaxPulseUs() const {
    return max_pulse_us;
}

u16 Servo::getServoUpdateFrequencyHz() const {
    return servo_update_frequency_hz;
}

u32 Servo::getFrameLengthMicroseconds() const {
    return frame_length_microseconds;
}

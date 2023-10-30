
#include <cstdint>
#include <thread>
#include <chrono>


#include "controller-config.h"
#include "namespace-stuffs.h"

#include "pca9685/pca9685.h"

#include "i2c_servo.h"

I2CServoController::I2CServoController(u8 i2cAddress) {
    trace("starting up a new I2CServoController on address 0x{0:x}", i2cAddress);

    this->i2cAddress = i2cAddress;
}

u8 I2CServoController::begin() {
    debug("starting begin()");

    debug("resetting the PCA9685");
    reset();

    debug("setting the oscillator frequency to {}", FREQUENCY_OSCILLATOR);
    this->setOscillatorFrequency(FREQUENCY_OSCILLATOR);

    debug("setting the PWM frequency");
    this->setPWMFrequency(1000);

    return 1;
}

void I2CServoController::reset() {
    debug("resetting the servoController on address 0x{0:x}", i2cAddress);

    // Make sure we're the only ones touching the servo controller
    std::lock_guard<std::mutex> lock(servo_mutex);

    write8(PCA9685_MODE1, MODE1_RESTART);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    trace("done with reset");
}

void I2CServoController::sleep() {
    debug("telling the PCA9685 on servoController at address 0x{0:x} to go to sleep 💤", i2cAddress);

    std::lock_guard<std::mutex> lock(servo_mutex);
    u8 awake = read8(PCA9685_MODE1);
    u8 sleep = awake | MODE1_SLEEP; // set sleep bit high
    write8(PCA9685_MODE1, sleep);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    trace("PCA9685 should now be taking a nap");
}

void I2CServoController::wakeup() {
    debug("waking up the PCA9685 on the servoController at address 0x{0:x} ☀️", i2cAddress);

    std::lock_guard<std::mutex> lock(servo_mutex);
    u8 sleep = read8(PCA9685_MODE1);
    u8 wakeup = sleep & ~MODE1_SLEEP; // set sleep bit low
    write8(PCA9685_MODE1, wakeup);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    trace("should be awake now!");
}

// This is taken from Adafruit's driver for this board, but converted to using
// the bcm driver on the Pi. Their version is for Arduino.
void I2CServoController::setPWMFrequency(float frequency) {

    debug("Attempting to set the frequency to {}", frequency);

    std::lock_guard<std::mutex> lock(servo_mutex);

    // Range output modulation frequency is dependant on oscillator
    if (frequency < 1)
        frequency = 1;
    if (frequency > 3500)
        frequency = 3500; // Datasheet limit is 3052=50MHz/(4*4096)

    float prescaleval = ((_oscillator_freq / (frequency * 4096.0)) + 0.5) - 1;
    if (prescaleval < PCA9685_PRESCALE_MIN)
        prescaleval = PCA9685_PRESCALE_MIN;
    if (prescaleval > PCA9685_PRESCALE_MAX)
        prescaleval = PCA9685_PRESCALE_MAX;
    u8 prescale = (uint8_t)prescaleval;

    debug("Final pre-scale: {}", prescale);


    uint8_t oldmode = read8(PCA9685_MODE1);
    uint8_t newmode = (oldmode & ~MODE1_RESTART) | MODE1_SLEEP; // sleep
    write8(PCA9685_MODE1, newmode);                             // go to sleep
    write8(PCA9685_PRESCALE, prescale); // set the prescaler
    write8(PCA9685_MODE1, oldmode);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // This sets the MODE1 register to turn on auto increment.
    write8(PCA9685_MODE1, oldmode | MODE1_RESTART | MODE1_AI);

    debug("Mode now is 0x{0:x}", read8(PCA9685_MODE1));

}

/*!
 *  @brief  Getter for the internally tracked oscillator used for freq
 * calculations
 *  @returns The frequency the PCA9685 thinks it is running at (it cannot
 * introspect)
 */
[[maybe_unused]] u32 I2CServoController::getOscillatorFrequency(void) const {
    return this->_oscillator_freq;
}

/*!
 *  @brief Setter for the internally tracked oscillator used for freq
 * calculations
 *  @param freq The frequency the PCA9685 should use for frequency calculations
 */
void I2CServoController::setOscillatorFrequency(u32 freq) {
    this->_oscillator_freq = freq;
    trace("set the oscillator frequency to {}", this->_oscillator_freq);
}
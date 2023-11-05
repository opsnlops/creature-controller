
#include <cstdint>
#include <thread>
#include <chrono>


#include "controller-config.h"
#include "namespace-stuffs.h"

#include "pca9685/pca9685.h"

#include "i2c_servo.h"

I2CServoController::I2CServoController(std::shared_ptr<I2CDevice> i2c, u8 i2cAddress)  {
    trace("starting up a new I2CServoController on address 0x{0:x}", i2cAddress);

    this->i2c = i2c;
    this->i2cAddress = i2cAddress;
}

u8 I2CServoController::getDeviceAddress() {
    return i2cAddress;
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
    trace("mutex lock for servo controller on address 0x{0:x} acquired", i2cAddress);

    i2c->write8(i2cAddress, PCA9685_MODE1, MODE1_RESTART);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    trace("done with reset");
}

void I2CServoController::sleep() {
    debug("telling the PCA9685 on servoController at address 0x{0:x} to go to sleep 💤", i2cAddress);

    std::lock_guard<std::mutex> lock(servo_mutex);
    trace("mutex lock for servo controller on address 0x{0:x} acquired", i2cAddress);

    u8 awake = i2c->read8(i2cAddress, PCA9685_MODE1);
    u8 sleep = awake | MODE1_SLEEP; // set sleep bit high
    i2c->write8(i2cAddress, PCA9685_MODE1, sleep);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    trace("PCA9685 should now be taking a nap");
}

void I2CServoController::wakeup() {
    debug("waking up the PCA9685 on the servoController at address 0x{0:x} ☀️", i2cAddress);

    std::lock_guard<std::mutex> lock(servo_mutex);
    trace("mutex lock for servo controller on address 0x{0:x} acquired", i2cAddress);

    u8 sleep = i2c->read8(i2cAddress, PCA9685_MODE1);
    u8 wakeup = sleep & ~MODE1_SLEEP; // set sleep bit low
    i2c->write8(i2cAddress, PCA9685_MODE1, wakeup);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    trace("should be awake now!");
}

// This is taken from Adafruit's driver for this board, but converted to using
// the bcm driver on the Pi. Their version is for Arduino.
void I2CServoController::setPWMFrequency(float frequency) {

    debug("Attempting to set the frequency to {}", frequency);

    std::lock_guard<std::mutex> lock(servo_mutex);
    trace("mutex lock for servo controller on address 0x{0:x} acquired", i2cAddress);

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


    uint8_t oldmode = i2c->read8(i2cAddress, PCA9685_MODE1);
    uint8_t newmode = (oldmode & ~MODE1_RESTART) | MODE1_SLEEP; // sleep
    i2c->write8(i2cAddress, PCA9685_MODE1, newmode);                             // go to sleep
    i2c->write8(i2cAddress, PCA9685_PRESCALE, prescale); // set the prescaler
    i2c->write8(i2cAddress, PCA9685_MODE1, oldmode);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // This sets the MODE1 register to turn on auto increment.
    i2c->write8(i2cAddress, PCA9685_MODE1, oldmode | MODE1_RESTART | MODE1_AI);

    debug("Mode now is 0x{0:x}", i2c->read8(i2cAddress, PCA9685_MODE1));

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
 *         calculations
 *  @param freq The frequency the PCA9685 should use for frequency calculations
 */
void I2CServoController::setOscillatorFrequency(u32 freq) {
    this->_oscillator_freq = freq;
    trace("set the oscillator frequency to {}", this->_oscillator_freq);
}


u8 I2CServoController::readPrescale() {

    u8 preScale = i2c->read8(i2cAddress, PCA9685_PRESCALE);
    trace("Pre-scale is currently {}", preScale);

    return preScale;
}

/*!
 *  @brief  Gets the PWM output of one of the PCA9685 pins
 *  @param  num One of the PWM output pins, from 0 to 15
 *  @param  off If true, returns PWM OFF value, otherwise PWM ON
 *  @return requested PWM output value
 */
u16 I2CServoController::getPWM(u8 num, bool off) {

    // TODO: Do we need to lock the mutex here?

    debug("getting PWM value on {}", num);

    char buffer[2] = {static_cast<char>(u8(PCA9685_LED0_ON_L + 4 * num)), 0};
    if (off)
        buffer[0] += 2;
    i2c->write_then_read(i2cAddress, buffer, 1, buffer, 2);
    u16 result = u16(buffer[0]) | (u16(buffer[1]) << 8);

    debug("determined that {} is on value {}", num, result);

    return result;
}

/*!
 *  @brief  Sets the PWM output of one of the PCA9685 pins
 *  @param  num One of the PWM output pins, from 0 to 15
 *  @param  on At what point in the 4096-part cycle to turn the PWM output ON
 *  @param  off At what point in the 4096-part cycle to turn the PWM output OFF
 *  @return 0 if successful, otherwise 1
 */
u8 I2CServoController::setPWM(u8 num, u16 on, u16 off) {

    debug("setting PWM number {} to on: {}, off: {}", num, on, off);


    std::lock_guard<std::mutex> lock(servo_mutex);
    trace("mutex lock for servo controller on address 0x{0:x} acquired", i2cAddress);


    char buffer[5];
    buffer[0] = PCA9685_LED0_ON_L + 4 * num;
    buffer[1] = on;
    buffer[2] = on >> 8;
    buffer[3] = off;
    buffer[4] = off >> 8;

    if (i2c->write(i2cAddress, buffer, 5)) {
        return 0;
    } else {
        return 1;
    }

}


/*!
 *   @brief  Helper to set pin PWM output. Sets pin without having to deal with
 *           on/off tick placement and properly handles a zero value as completely off and
 *           4095 as completely on.  Optional invert parameter supports inverting the
 *           pulse for sinking to ground.
 *   @param  num One of the PWM output pins, from 0 to 15
 *   @param  val The number of ticks out of 4096 to be active, should be a value
 *               from 0 to 4095 inclusive.
 *   @param  invert If true, inverts the output, defaults to 'false'
 */
void I2CServoController::setPin(u8 num, u16 val, bool invert) {

    debug("setting pin {} to value {} (invert: {})", num, val, invert);

    // Clamp value between 0 and 4095 inclusive.
    val = std::min(val, (u16)4095);
    if (invert) {
        if (val == 0) {
            // Special value for signal fully on.
            setPWM(num, 4096, 0);
        } else if (val == 4095) {
            // Special value for signal fully off.
            setPWM(num, 0, 4096);
        } else {
            setPWM(num, 0, 4095 - val);
        }
    } else {
        if (val == 4095) {
            // Special value for signal fully on.
            setPWM(num, 4096, 0);
        } else if (val == 0) {
            // Special value for signal fully off.
            setPWM(num, 0, 4096);
        } else {
            setPWM(num, 0, val);
        }
    }
}


#include <FreeRTOS.h>
#include <task.h>


#include "logging/logging.h"

#include "controller-config.h"

#include "device/pca9685.h"

/*
 * This is an interface to the PCA9685 PWM controller
 *
 * It is based on Adafruit's PCA9685 library, but is written in C and is
 * geared for the Pi Pico.
 *
 * Adafruit's library can be found at
 * https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library/
 *
 * Adafruit's library is licensed under the BSD license.
 *
 */

u32 pca9685_oscillator_freq = FREQUENCY_OSCILLATOR;

#define PCA9685_MIN(a, b) (((a) < (b)) ? (a) : (b))

bool pca9685_begin(i2c_inst_t *i2c_interface, u8 prescale) {

    // set the default internal frequency
    pca9685_setOscillatorFrequency(FREQUENCY_OSCILLATOR);

    if (prescale != 0) {
        pca9685_setExtClk(i2c_interface, prescale);
    } else {
        // set a default frequency
        pca9685_setPWMFreq(i2c_interface, 1000);
    }

    debug("called pca9685_begin with prescale %u", prescale);
    return true;
}

void pca9685_reset(i2c_inst_t *i2c_interface) {
    pca9685_write8(i2c_interface, PCA9685_MODE1, MODE1_RESTART);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void pca9685_sleep(i2c_inst_t *i2c_interface) {
    u8 awake = pca9685_read8(i2c_interface, PCA9685_MODE1);
    u8 sleep = awake | MODE1_SLEEP; // set sleep bit high
    pca9685_write8(i2c_interface, PCA9685_MODE1, sleep);
    vTaskDelay(pdMS_TO_TICKS(5));
}

void pca9685_wakeup(i2c_inst_t *i2c_interface) {
    u8 sleep = pca9685_read8(i2c_interface, PCA9685_MODE1);
    u8 wakeup = sleep & ~MODE1_SLEEP; // set sleep bit low
    pca9685_write8(i2c_interface, PCA9685_MODE1, wakeup);
}

void pca9685_setExtClk(i2c_inst_t *i2c_interface, u8 prescale) {

    debug("calling pca9685_setExtClk with prescale %u", prescale);

    u8 old_mode = pca9685_read8(i2c_interface, PCA9685_MODE1);
    u8 new_mode = (old_mode & ~MODE1_RESTART) | MODE1_SLEEP; // sleep
    pca9685_write8(i2c_interface, PCA9685_MODE1, new_mode); // go to sleep, turn off internal oscillator

    // This sets both the SLEEP and EXTCLK bits of the MODE1 register to switch to
    // use the external clock.
    pca9685_write8(i2c_interface, PCA9685_MODE1, (new_mode |= MODE1_EXTCLK));

    pca9685_write8(i2c_interface, PCA9685_PRESCALE, prescale); // set the prescaler

    vTaskDelay(pdMS_TO_TICKS(5));

    // clear the SLEEP bit to start
    pca9685_write8(i2c_interface, PCA9685_MODE1, (new_mode & ~MODE1_SLEEP) | MODE1_RESTART | MODE1_AI);


    // Get the new mode from the PCA9685
    u8 mode_from_pca9685 = pca9685_read8(i2c_interface, PCA9685_MODE1);
    info("pca9685_setExtClk: mode is now 0x%02X", mode_from_pca9685);

}

void pca9685_setPWMFreq(i2c_inst_t *i2c_interface, float freq) {
   debug("setting the pca9685 PWM frequency to %f", freq);

    // Range output modulation frequency is dependant on oscillator
    if (freq < 1)
        freq = 1;
    if (freq > 3500)
        freq = 3500; // Datasheet limit is 3052=50MHz/(4*4096)

    float prescaleval = ((pca9685_oscillator_freq / (freq * 4096.0)) + 0.5) - 1;
    if (prescaleval < PCA9685_PRESCALE_MIN)
        prescaleval = PCA9685_PRESCALE_MIN;
    if (prescaleval > PCA9685_PRESCALE_MAX)
        prescaleval = PCA9685_PRESCALE_MAX;
    u8 prescale = (u8)prescaleval;

    debug("final prescale %u", prescale);


    u8 old_mode = pca9685_read8(i2c_interface, PCA9685_MODE1);
    u8 new_mode = (old_mode & ~MODE1_RESTART) | MODE1_SLEEP; // sleep
    pca9685_write8(i2c_interface, PCA9685_MODE1, new_mode);                            // go to sleep
    pca9685_write8(i2c_interface, PCA9685_PRESCALE, prescale); // set the prescaler
    pca9685_write8(i2c_interface, PCA9685_MODE1, old_mode);
    vTaskDelay(pdMS_TO_TICKS(5));
    // This sets the MODE1 register to turn on auto increment.
    pca9685_write8(i2c_interface, PCA9685_MODE1, old_mode | MODE1_RESTART | MODE1_AI);

    // Get the new mode from the PCA9685
    u8 mode_from_pca9685 = pca9685_read8(i2c_interface, PCA9685_MODE1);
    info("pca9685_setPWMFreq: mode is now 0x%02X", mode_from_pca9685);

}

void pca9685_setOutputMode(i2c_inst_t *i2c_interface, bool totempole) {
    u8 old_mode = pca9685_read8(i2c_interface, PCA9685_MODE2);
    u8 new_mode;
    if (totempole) {
        new_mode = old_mode | MODE2_OUTDRV;
    } else {
        new_mode = old_mode & ~MODE2_OUTDRV;
    }
    pca9685_write8(i2c_interface, PCA9685_MODE2, new_mode);

    info("pca9685_setOutputMode: Setting output mode: %s by setting MODE2 to 0x%02X",
         totempole ? "totempole" : "open drain",
         new_mode);

}

u16 pca9685_getPWM(i2c_inst_t *i2c_interface, u8 num, bool off) {
    u8 buffer[2] = {(u8)(PCA9685_LED0_ON_L + 4 * num), 0};
    if (off)
        buffer[0] += 2;
    i2c_write_blocking(i2c_interface, SERVO_MODULE_I2C_ADDRESS, buffer, 1, false);
    i2c_read_blocking(i2c_interface, SERVO_MODULE_I2C_ADDRESS, buffer, 2, false);

    return (u16)(buffer[0]) | (u16)(buffer[1] << 8);

}


u8 pca9685_setPWM(i2c_inst_t *i2c_interface, u8 num, u16 on, u16 off) {

    debug("Setting PWM %u: %u->%u", num, on, off);

    u8 buffer[5];
    buffer[0] = PCA9685_LED0_ON_L + 4 * num;
    buffer[1] = on;
    buffer[2] = on >> 8;
    buffer[3] = off;
    buffer[4] = off >> 8;

    if (i2c_write_blocking(i2c_interface, SERVO_MODULE_I2C_ADDRESS, buffer, 5, false)) {
        return 0;
    } else {
        return 1;
    }
}

void pca9685_setPin(i2c_inst_t *i2c_interface, u8 num, u16 val, bool invert) {
    // Clamp value between 0 and 4095 inclusive.
    val = PCA9685_MIN(val, (u16)4095);
    if (invert) {
        if (val == 0) {
            // Special value for signal fully on.
            pca9685_setPWM(i2c_interface, num, 4096, 0);
        } else if (val == 4095) {
            // Special value for signal fully off.
            pca9685_setPWM(i2c_interface, num, 0, 4096);
        } else {
            pca9685_setPWM(i2c_interface, num, 0, 4095 - val);
        }
    } else {
        if (val == 4095) {
            // Special value for signal fully on.
            pca9685_setPWM(i2c_interface, num, 4096, 0);
        } else if (val == 0) {
            // Special value for signal fully off.
            pca9685_setPWM(i2c_interface, num, 0, 4096);
        } else {
            pca9685_setPWM(i2c_interface, num, 0, val);
        }
    }
}

u8 pca9685_readPrescale(i2c_inst_t *i2c_interface) {
    return pca9685_read8(i2c_interface, PCA9685_PRESCALE);
}

void pca9685_writeMicroseconds(i2c_inst_t *i2c_interface, u8 num, u16 Microseconds) {

    debug("Setting PWM Via Microseconds on output %u: %u", num, Microseconds);

    double pulse = Microseconds;
    double pulselength;
    pulselength = 1000000; // 1,000,000 us per second

    // TODO: Wow why are they doing this

    // Read prescale
    uint16_t prescale = pca9685_readPrescale(i2c_interface);

    debug("PCA9685 chip prescale: %u", prescale);

    // Calculate the pulse for PWM based on Equation 1 from the datasheet section
    // 7.3.5
    prescale += 1;
    pulselength *= prescale;
    pulselength /= pca9685_oscillator_freq;

    debug("Pulselength: %f", pulselength);

    pulse /= pulselength;

    debug("Pulse for PWM: %f", pulse);

    pca9685_setPWM(i2c_interface,num, 0, pulse);
}


void pca9685_setOscillatorFrequency(u32 freq) {
    pca9685_oscillator_freq = freq;
}

u32 pca9685_getOscillatorFrequency(void) {
    return pca9685_oscillator_freq;
}


u8 pca9685_read8(i2c_inst_t *i2c_interface, u8 registerAddress) {

    u8 data = 0;

    // Write the register that we wish to read, and then read back the result
    i2c_write_blocking(i2c_interface, SERVO_MODULE_I2C_ADDRESS, &registerAddress, 1, false);
    i2c_read_blocking(i2c_interface, SERVO_MODULE_I2C_ADDRESS, &data, 1, false);

    return data;

}


void pca9685_write8(i2c_inst_t *i2c_interface, u8 registerAddress, u8 data) {

    // Create a tiny buffer for this data, and then send it
    u8 buffer[2] = {registerAddress, data};
    i2c_write_blocking(i2c_interface, SERVO_MODULE_I2C_ADDRESS, buffer, 2, false);

}

#pragma once

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

#include "hardware/i2c.h"


#include "controller-config.h"


// REGISTER ADDRESSES
#define PCA9685_MODE1 0x00      /**< Mode Register 1 */
#define PCA9685_MODE2 0x01      /**< Mode Register 2 */
#define PCA9685_SUBADR1 0x02    /**< I2C-bus subaddress 1 */
#define PCA9685_SUBADR2 0x03    /**< I2C-bus subaddress 2 */
#define PCA9685_SUBADR3 0x04    /**< I2C-bus subaddress 3 */
#define PCA9685_ALLCALLADR 0x05 /**< LED All Call I2C-bus address */
#define PCA9685_LED0_ON_L 0x06  /**< LED0 on tick, low byte*/
#define PCA9685_LED0_ON_H 0x07  /**< LED0 on tick, high byte*/
#define PCA9685_LED0_OFF_L 0x08 /**< LED0 off tick, low byte */
#define PCA9685_LED0_OFF_H 0x09 /**< LED0 off tick, high byte */
// etc all 16:  LED15_OFF_H 0x45
#define PCA9685_ALLLED_ON_L 0xFA  /**< load all the LEDn_ON registers, low */
#define PCA9685_ALLLED_ON_H 0xFB  /**< load all the LEDn_ON registers, high */
#define PCA9685_ALLLED_OFF_L 0xFC /**< load all the LEDn_OFF registers, low */
#define PCA9685_ALLLED_OFF_H 0xFD /**< load all the LEDn_OFF registers,high */
#define PCA9685_PRESCALE 0xFE     /**< Prescaler for PWM output frequency */
#define PCA9685_TESTMODE 0xFF     /**< defines the test mode to be entered */

// MODE1 bits
#define MODE1_ALLCAL 0x01  /**< respond to LED All Call I2C-bus address */
#define MODE1_SUB3 0x02    /**< respond to I2C-bus subaddress 3 */
#define MODE1_SUB2 0x04    /**< respond to I2C-bus subaddress 2 */
#define MODE1_SUB1 0x08    /**< respond to I2C-bus subaddress 1 */
#define MODE1_SLEEP 0x10   /**< Low power mode. Oscillator off */
#define MODE1_AI 0x20      /**< Auto-Increment enabled */
#define MODE1_EXTCLK 0x40  /**< Use EXTCLK pin clock */
#define MODE1_RESTART 0x80 /**< Restart enabled */
// MODE2 bits
#define MODE2_OUTNE_0 0x01 /**< Active LOW output enable input */
#define MODE2_OUTNE_1 0x02 /**< Active LOW output enable input - high impedience */
#define MODE2_OUTDRV 0x04 /**< totem pole structure vs open-drain */
#define MODE2_OCH 0x08    /**< Outputs change on ACK vs STOP */
#define MODE2_INVRT 0x10  /**< Output logic state inverted */

#define PCA9685_FREQUENCY_OSCILLATOR 25000000 /**< Int. osc. frequency in datasheet */

#define PCA9685_PRESCALE_MIN 3   /**< minimum prescale value */
#define PCA9685_PRESCALE_MAX 255 /**< maximum prescale value */

#define PCA9685_PWM_RANGE 4096


bool pca9685_begin(i2c_inst_t *i2c_interface, u8 prescale);
void pca9685_reset(i2c_inst_t *i2c_interface);
void pca9685_sleep(i2c_inst_t *i2c_interface);
void pca9685_wakeup(i2c_inst_t *i2c_interface);
void pca9685_setExtClk(i2c_inst_t *i2c_interface, u8 prescale);
void pca9685_setPWMFreq(i2c_inst_t *i2c_interface, float freq);
void pca9685_setOutputMode(i2c_inst_t *i2c_interface, bool totempole);
u16 pca9685_getPWM(i2c_inst_t *i2c_interface, u8 num, bool off);
u8 pca9685_setPWM(i2c_inst_t *i2c_interface, u8 num, u16 on, u16 off);
void pca9685_setPin(i2c_inst_t *i2c_interface, u8 num, u16 val, bool invert);
u8 pca9685_readPrescale(i2c_inst_t *i2c_interface);

void pca9685_set_prescale(i2c_inst_t *i2c_interface, u8 prescale);

/*
 * Create a tiny HAL interface for the PCA9685, the same way
 * that Adafruit does it
 */

/**
 * Read an eight bit value from the PCA9685
 *
 * @param i2c_interface the i2c interface to use
 * @param registerAddress the register to read
 * @return the data in the register
 */
u8 pca9685_read8(i2c_inst_t *i2c_interface, u8 registerAddress);


/**
 * Write an eight bit value to the PCA9685
 *
 * @param i2c_interface the i2c interface to use
 * @param registerAddress the register to write to
 * @param data the byte of data to write
 */
void pca9685_write8(i2c_inst_t *i2c_interface, u8 registerAddress, u8 data);

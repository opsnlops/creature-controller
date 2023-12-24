
#pragma once

#include <memory>


// Our modules
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#include <FreeRTOS.h>
#include <tasks.h>


#include "controller-config.h"

// These are the modules on the controller card (A and B, C is currently not used)
#define CONTROLLER_NUM_MODULES 2

// The max number of motors per module (0, 1, 2, 3)
#define CONTROLLER_MOTORS_PER_MODULE 4

// Eight servos at the moment. (C is not used)
#define SERVO_0_GPIO_PIN            6               // Pin 9,  PMW  3A
#define SERVO_1_GPIO_PIN            7               // Pin 10, PWM  3B
#define SERVO_2_GPIO_PIN            8               // Pin 11, PWM  4A
#define SERVO_3_GPIO_PIN            9               // Pin 12, PWM  4B
#define SERVO_4_GPIO_PIN            10              // Pin 14, PWM  5A
#define SERVO_5_GPIO_PIN            11              // Pin 15, PWM  5B
#define SERVO_6_GPIO_PIN            12              // Pin 16, PWM  6A
#define SERVO_7_GPIO_PIN            13              // Pin 17, PWM  6B

#define INVALID_MOTOR_ID            255

namespace creatures::controller {

    // Defines a mapping between motor IDs and GPIO pins
    typedef struct {
        const char* motor_id;
        uint gpio_pin;
        uint slice;
        uint channel;
    } MotorMap;


    /**
     * @brief Gets the GPIO pin for a given motor ID from the controller
     *
     * @param motor_id the motor ID from the controller (ie A0, A2, B0, B2, etc)
     * @return a `uint` with the GPIO pin, or INVALID_MOTOR_ID if not found
     */
    uint getGpioPin(const char* motor_id);


    // ISR, called when the PWM wraps
    void __isr on_pwm_wrap_handler();


    void init();
    void start();


    bool requestServoPosition(const char* motor_id, u16 requestedPosition);




    /**
     * @brief Sets the frequency on a PWM channel. Returns the resolution of the slice.
     *
     * The Pi Pico has a 125Mhz clock on the PWM pins. This is far too fast for a servo
     * to work with (most run at 50Hz). This function figures out the correct dividers to
     * use to drop it down to the given frequency.
     *
     * It will return the wrap value for the counter, which can be thought of as the resolution
     * of the channel. (ie, wrap / 2 = 50% duty cycle)
     *
     *
     * This bit of code was taken from the book:
     *    Fairhead, Harry. Programming The Raspberry Pi Pico In C (p. 122). I/O Press. Kindle Edition.
     *
     * @param slice_num The PWM slice number
     * @param chan The PWM channel number
     * @param frequency The frequency of updates (50Hz is normal)
     * @param d speed (currently unused)
     * @return the wrap counter wrap value for this slice an channel (aka the resolution)
     */
    u32 pwm_set_freq_duty(uint slice_num, uint chan, uint32_t frequency, int d);


} // creatures::controller


#pragma once

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

// Defines a mapping between motor IDs and GPIO pins
typedef struct {
    const char* motor_id;
    uint gpio_pin;
    uint slice;
    uint channel;
    u16 requested_position;
} MotorMap;


u8 getMotorMapIndex(const char* motor_id);


// ISR, called when the PWM wraps
void __isr on_pwm_wrap_handler();


void controller_init();
void controller_start();


/**
 * @brief Request that a servo's PWM counter be set to a desired number of microseconds
 *
 * Servos work via a PWM counter. This counter defines the position of the servo. Each
 * servo has a range that it maps a number of microseconds of a pulse at the start of a
 * frame to.
 *
 * The microsecond range that each servo works over is defined from two important things:
 *
 *    * The range specified by the servo's manufacturer
 *    * The range each of our servos are configured to work over
 *
 * This is pretty critical. If we try to go beyond the number of microseconds that our
 * configuration is set to, it's likely that we're going to break something. The servos
 * in use are quite strong, and most of the creatures are made out of 3D printed parts, ie
 * they're just plastic.
 *
 *
 * But there's more to this story.
 *
 * The Pi Pico uses a PWM counter. It doesn't really work by microseconds. It has a counter
 * and it turns the channel on if the counter is lower than the requested value, and it turns
 * it off if it's higher. That's all it does.
 *
 * What this function does is map the microseconds requested to the number of ticks on the
 * counter when the channel should go from on to off.
 *
 * It was a design decision to keep this in the firmware itself. Right now I have no plans
 * to expand beyond Pi Picos, but if some new microcontroller comes along (or even a new
 * version of the Pi Pico), it'll work differently. I'm using this piece of low level code
 * to do the actual translation from microseconds to ticks, because it's very specific to
 * this one microcontroller.
 *
 * @param motor_id The motor ID to adjust (ie A0, A1, C0, etc)
 * @param requestedMicroseconds The number of microseconds to request
 * @return true if all error checks passed, or false otherwise
 */
bool requestServoPosition(const char* motor_id, u16 requestedMicroseconds);




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

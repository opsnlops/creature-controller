

#include <limits.h>

// Our modules
#include "hardware/gpio.h"
#include "hardware/pwm.h"


#include "logging/logging.h"

#include "controller-config.h"
#include "controller.h"


// Stats
volatile u64 number_of_pwm_wraps = 0UL;

/*
 * The following map is used to map motor IDs to GPIO pins!
 *
 * The bit shifts come from the Pico SDK. In order to make this map be created at build
 * time they needed to be a constant value, so I copied them here.
 *
 * See `pwm.h` in the Pico SDK for more information! 😅
 *
 */

// Mapping of motor IDs to GPIO pins
MotorMap motor_map[CONTROLLER_NUM_MODULES * CONTROLLER_MOTORS_PER_MODULE] = {
        {"A0", SERVO_0_GPIO_PIN, (SERVO_0_GPIO_PIN >> 1u) & 7u, SERVO_0_GPIO_PIN & 1u},
        {"A1", SERVO_1_GPIO_PIN, (SERVO_1_GPIO_PIN >> 1u) & 7u, SERVO_1_GPIO_PIN & 1u},
        {"A2", SERVO_2_GPIO_PIN, (SERVO_2_GPIO_PIN >> 1u) & 7u, SERVO_2_GPIO_PIN & 1u},
        {"A3", SERVO_3_GPIO_PIN, (SERVO_3_GPIO_PIN >> 1u) & 7u, SERVO_3_GPIO_PIN & 1u},
        {"B0", SERVO_4_GPIO_PIN, (SERVO_4_GPIO_PIN >> 1u) & 7u, SERVO_4_GPIO_PIN & 1u},
        {"B1", SERVO_5_GPIO_PIN, (SERVO_5_GPIO_PIN >> 1u) & 7u, SERVO_5_GPIO_PIN & 1u},
        {"B2", SERVO_6_GPIO_PIN, (SERVO_6_GPIO_PIN >> 1u) & 7u, SERVO_6_GPIO_PIN & 1u},
        {"B3", SERVO_7_GPIO_PIN, (SERVO_7_GPIO_PIN >> 1u) & 7u, SERVO_7_GPIO_PIN & 1u}
};


/**
 * We need to know how long each frame is in microseconds. This is set when the
 * first servo is configured. It's _possible_ for the Pi Pico to run PWM channel at
 * a different frequency, but we don't do that. It doesn't really make sense to do it,
 * since almost all servos work at 50Hz.
 */
u64 frame_length_microseconds = 0UL;

/**
 * What's the size of the PWM counter?
 */
u32 pwm_resolution = 0UL;


void controller_init() {
    info("init-ing the controller");

}

void controller_start() {
    info("starting the controller");

    // Fire up PWM
    u32 wrap;
    for (size_t i = 0; i < sizeof(motor_map) / sizeof(motor_map[0]); ++i) {
        gpio_set_function(motor_map[i].gpio_pin, GPIO_FUNC_PWM);
        wrap = pwm_set_freq_duty(motor_map[i].slice, motor_map[i].channel, 50, 0);
        pwm_set_enabled(motor_map[i].slice, true);
    }

    /*
     * If this is the first one, set the frame length and resolution
     */
    if (frame_length_microseconds == 0UL) {
        frame_length_microseconds = 1000000UL / 50UL;  // TODO: This is assuming 50Hz
        pwm_resolution = wrap;
    }

    // Install the IRQ handler for the servos. Use servo 0 as a proxy for the rest.
    pwm_set_irq_enabled(motor_map[0].slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap_handler);
    irq_set_enabled(PWM_IRQ_WRAP, true);
}

u8 getMotorMapIndex(const char *motor_id) {
    if (motor_id == NULL || motor_id[1] == '\0') return INVALID_MOTOR_ID;

    u8 module = motor_id[0] - 'A';         // Convert 'A', 'B', 'C' to 0, 1, 2
    u8 motor_number = motor_id[1] - '0';   // Convert '0', '1', ..., '3' to 0, 1, ..., 3

    // Make sure the controller requested a valid motor
    if (module < 0 || module >= CONTROLLER_NUM_MODULES || motor_number < 0 ||
        motor_number >= CONTROLLER_MOTORS_PER_MODULE) {
        warning("Invalid motor ID: %s", motor_id);
        return INVALID_MOTOR_ID;
    }

    u8 index = module * CONTROLLER_MOTORS_PER_MODULE + motor_number;
    return index;
}


bool requestServoPosition(const char *motor_id, u16 requestedMicroseconds) {
    if (motor_id == NULL || motor_id[1] == '\0') return false;

    // Get the index in the array
    u8 motor_id_index = getMotorMapIndex(motor_id);
    if (motor_id_index == INVALID_MOTOR_ID) {
        warning("Invalid motor ID: %s", motor_id);
        return false;
    }

    // What percentage of the frame is going to be set to on?
    double frame_active = (float) requestedMicroseconds / (float) frame_length_microseconds;

    // ..and what counter value is that?
    u32 desired_ticks = (u32) ((float) pwm_resolution * frame_active);

    verbose("Requested position for %s: %u ticks -> %u microseconds", motor_id, desired_ticks, requestedMicroseconds);
    motor_map[motor_id_index].requested_position = desired_ticks;

    return true;
}

void __isr

on_pwm_wrap_handler() {

    // This is an ISR. Treat with caution! ☠️

    for (size_t i = 0; i < sizeof(motor_map) / sizeof(motor_map[0]); ++i) {
        pwm_set_chan_level(motor_map[i].slice,
                           motor_map[i].channel,
                           motor_map[i].requested_position);
    }

    pwm_clear_irq(motor_map[0].slice);
    number_of_pwm_wraps = number_of_pwm_wraps + 1UL;
}


u32 pwm_set_freq_duty(uint slice_num, uint chan, u32 frequency, int d) {
    u32 clock = 125000000;
    u32 divider16 = clock / frequency / 4096 + (clock % (frequency * 4096) != 0);
    if (divider16 / 16 == 0) divider16 = 16;
    u32 wrap = clock * 16 / divider16 / frequency - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * d / 100);
    return wrap;
}

/**
 * Called from a timer in usb.c when the CDC is connected
 */
void controller_connected() {

}

/**
 * Called from a timer in usb.c when the CDC is disconnected
 */
void controller_disconnected() {

}

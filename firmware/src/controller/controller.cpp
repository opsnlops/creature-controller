

#include <climits>

#include <FreeRTOS.h>

// Our modules
#include "hardware/gpio.h"
#include "hardware/pwm.h"



#include "logging/logging.h"

#include "controller-config.h"
#include "controller.h"

#define SERVO_FREQUENCY 50


// Stats
volatile u64 number_of_pwm_wraps = 0UL;

namespace creatures::controller {


    // Mapping of motor IDs to GPIO pins
    MotorMap motor_map[CONTROLLER_NUM_MODULES * CONTROLLER_MOTORS_PER_MODULE] = {
            {"A0", SERVO_0_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_0_GPIO_PIN), pwm_gpio_to_channel(SERVO_0_GPIO_PIN)},
            {"A1", SERVO_1_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_1_GPIO_PIN), pwm_gpio_to_channel(SERVO_1_GPIO_PIN)},
            {"A2", SERVO_2_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_2_GPIO_PIN), pwm_gpio_to_channel(SERVO_2_GPIO_PIN)},
            {"A3", SERVO_3_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_3_GPIO_PIN), pwm_gpio_to_channel(SERVO_3_GPIO_PIN)},
            {"B0", SERVO_4_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_4_GPIO_PIN), pwm_gpio_to_channel(SERVO_4_GPIO_PIN)},
            {"B1", SERVO_5_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_5_GPIO_PIN), pwm_gpio_to_channel(SERVO_5_GPIO_PIN)},
            {"B2", SERVO_6_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_6_GPIO_PIN), pwm_gpio_to_channel(SERVO_6_GPIO_PIN)},
            {"B3", SERVO_7_GPIO_PIN, pwm_gpio_to_slice_num(SERVO_7_GPIO_PIN), pwm_gpio_to_channel(SERVO_7_GPIO_PIN)}
    };


    void init() {
        info("init-ing the controller");

    }

    void start() {
        info("starting the controller");

        // Fire up PWM
        for (auto & i : motor_map) {
            gpio_set_function(i.gpio_pin, GPIO_FUNC_PWM);
            //pwm_set_freq_duty(i.slice, i.channel, 1000000 / SERVO_FREQUENCY, 0);
            pwm_set_freq_duty(i.slice, i.channel, 50, 0);
            pwm_set_enabled(i.slice, true);
        }

        // Install the IRQ handler for the servos
        pwm_set_irq_enabled(motor_map[0].slice, true);

        irq_set_exclusive_handler(PWM_IRQ_WRAP, creatures::controller::on_pwm_wrap_handler);
        irq_set_enabled(PWM_IRQ_WRAP, true);
    }

    u8 getMotorMapIndex(const char* motor_id) {
        if(motor_id == nullptr || motor_id[1] == '\0') return INVALID_MOTOR_ID;

        u8 module = motor_id[0] - 'A';         // Convert 'A', 'B', 'C' to 0, 1, 2
        u8 motor_number = motor_id[1] - '0';   // Convert '0', '1', ..., '3' to 0, 1, ..., 3

        // Make sure the controller requested a valid motor
        if(module < 0 || module >= CONTROLLER_NUM_MODULES || motor_number < 0 || motor_number >= CONTROLLER_MOTORS_PER_MODULE) {
            warning("Invalid motor ID: %s", motor_id);
            return INVALID_MOTOR_ID;
        }

        u8 index = module * CONTROLLER_MOTORS_PER_MODULE + motor_number;
        return index;
    }


    bool requestServoPosition(const char* motor_id, u16 requestedPosition) {
        if(motor_id == nullptr || motor_id[1] == '\0') return false;

        // Get the index in the array
        u8 motor_id_index = getMotorMapIndex(motor_id);
        if(motor_id_index == INVALID_MOTOR_ID) {
            warning("Invalid motor ID: %s", motor_id);
            return false;
        }

        motor_map[motor_id_index].requested_position = requestedPosition;

        return true;
    }

    void __isr on_pwm_wrap_handler() {

        // This is an ISR. Treat with caution! ☠️

        for (auto & i : motor_map) {
            pwm_set_chan_level(i.slice,
                               i.channel,
                               i.requested_position);
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


} // creatures::controller

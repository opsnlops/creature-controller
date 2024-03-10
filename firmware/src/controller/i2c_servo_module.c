
#include <FreeRTOS.h>
#include <task.h>


#include <math.h>

#include "controller/i2c_servo_module.h"
#include "controller/controller.h"

#include "io/i2c.h"
#include "device/pca9685.h"

#include "logging/logging.h"

TaskHandle_t i2c_servo_module_task_handle;

extern MotorMap motor_map[MOTOR_MAP_SIZE];
extern volatile bool controller_safe_to_run;


/*
 * There's really no need to calculate out the pre-scaler and everything each time. It's not going to
 * change. The value to use is 121 for 50 Hz.
 */

#define OSCILLATOR_CLOCK_FREQUENCY 25000000  // 25 MHz
#define PRESCALE_VALUE 132              // This is for 50Hz
#define TOTAL_TICKS 4096

#define DURATION_OF_ONE_TICK_US (5.32)

void i2c_servo_module_init() {
    info("initializing i2c servo module");

}


void i2c_servo_module_start() {
    info("starting i2c servo module");

    xTaskCreate(i2c_servo_module_task,
                "i2c_servo_module_task",
                1512,
                NULL,
                1,
                &i2c_servo_module_task_handle);

}

u16 calculate_ticks_for_pulse(u16 pulse_width_us) {

    // Calculate and return the number of ticks directly, using integer arithmetic
    u16 ticks = (u16)((pulse_width_us + (DURATION_OF_ONE_TICK_US / 2)) / DURATION_OF_ONE_TICK_US);

    debug("mapped %u us to %u ticks", pulse_width_us, ticks);

    return ticks;
}



portTASK_FUNCTION(i2c_servo_module_task, pvParameters) {

    debug("hello from i2c servo module task");

    // Wake up the controller board
    debug("resetting the i2c servo module");
    pca9685_reset(SENSORS_I2C_BUS);
    //pca9685_begin(SERVO_MODULE_I2C_BUS, 0); // No pre-scale means use the internal clock

    // Set the prescaler
    pca9685_set_prescale(SENSORS_I2C_BUS, PRESCALE_VALUE);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        if (controller_safe_to_run) {
            for (size_t i = 0; i < sizeof(motor_map) / sizeof(motor_map[0]); ++i) {

                u16 ticks = calculate_ticks_for_pulse(motor_map[i].current_microseconds);
                pca9685_setPWM(SENSORS_I2C_BUS, motor_map[i].gpio_pin - 6, 0, ticks);

                //pca9685_setPWM(SERVO_MODULE_I2C_BUS, motor_map[i].gpio_pin - 6, 0, 2048);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
#pragma clang diagnostic pop
}

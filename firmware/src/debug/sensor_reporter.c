
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "controller/controller.h"
#include "debug/sensor_reporter.h"
#include "device/pac1954.h"
#include "io/message_processor.h"
#include "io/responsive_analog_read_filter.h"

#include "controller-config.h"

extern double board_temperature;
extern analog_filter sensed_motor_position[CONTROLLER_MOTORS_PER_MODULE];
extern motor_power_data_t motor_power_data[I2C_DEVICE_PAC1954_SENSOR_COUNT];

void start_sensor_reporter() {

    TimerHandle_t statsReportTimer = xTimerCreate(
            "SensorReportTimer",              // Timer name
            pdMS_TO_TICKS(5 * 1000),            // Timer period (20 seconds)
            pdTRUE,                          // Auto-reload
            (void *) 0,                        // Timer ID (not used here)
            sensorReportTimerCallback         // Callback function
    );

    if (statsReportTimer != NULL) {
        xTimerStart(statsReportTimer, 0);
    }

}


void sensorReportTimerCallback(TimerHandle_t xTimer) {

    // Create a buffer
    char message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH];
    memset(&message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

    snprintf(message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH,
             "SENSOR\tTEMP %.2f\tM0 %u %.2f %.2f %.2f\tM1 %u %.2f %.2f %.2f\tM2 %u %.2f %.2f %.2f\tM3 %u %.2f %.2f %.2f",
             board_temperature,
             analog_filter_get_value(&sensed_motor_position[0]),
             motor_power_data[0].voltage,
             motor_power_data[0].current,
             motor_power_data[0].power,
             analog_filter_get_value(&sensed_motor_position[1]),
             motor_power_data[1].voltage,
             motor_power_data[1].current,
             motor_power_data[1].power,
             analog_filter_get_value(&sensed_motor_position[2]),
             motor_power_data[2].voltage,
             motor_power_data[2].current,
             motor_power_data[2].power,
             analog_filter_get_value(&sensed_motor_position[3]),
             motor_power_data[3].voltage,
             motor_power_data[3].current,
             motor_power_data[3].power);

    send_to_controller(message);

    // Log some debug information, too
    debug("sensors: chassis: %.2fF, motor 0: pos: %u, %.2fV %.2fA %.2fW",
          board_temperature,
          analog_filter_get_value(&sensed_motor_position[0]),
          motor_power_data[0].voltage,
          motor_power_data[0].current,
          motor_power_data[0].power);
}
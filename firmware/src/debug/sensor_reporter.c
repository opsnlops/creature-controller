
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

#include "controller/config.h"
#include "types.h"

extern double board_temperature;
extern analog_filter sensed_motor_position[CONTROLLER_MOTORS_PER_MODULE];
extern sensor_power_data_t sensor_power_data[I2C_PAC1954_SENSOR_COUNT];

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

    // Create a buffer for the motor messages
    char motor_message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH];
    memset(&motor_message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

    snprintf(motor_message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH,
             "MSENSE\tM0 %u %.2f %.2f %.2f\tM1 %u %.2f %.2f %.2f\tM2 %u %.2f %.2f %.2f\tM3 %u %.2f %.2f %.2f\tM4 %u %.2f %.2f %.2f\tM5 %u %.2f %.2f %.2f\tM6 %u %.2f %.2f %.2f\tM7 %u %.2f %.2f %.2f",
             analog_filter_get_value(&sensed_motor_position[0]),
             sensor_power_data[0].voltage,
             sensor_power_data[0].current,
             sensor_power_data[0].power,
             analog_filter_get_value(&sensed_motor_position[1]),
             sensor_power_data[1].voltage,
             sensor_power_data[1].current,
             sensor_power_data[1].power,
             analog_filter_get_value(&sensed_motor_position[2]),
             sensor_power_data[2].voltage,
             sensor_power_data[2].current,
             sensor_power_data[2].power,
             analog_filter_get_value(&sensed_motor_position[3]),
             sensor_power_data[3].voltage,
             sensor_power_data[3].current,
             sensor_power_data[3].power,
             analog_filter_get_value(&sensed_motor_position[4]),
             sensor_power_data[4].voltage,
             sensor_power_data[4].current,
             sensor_power_data[4].power,
             analog_filter_get_value(&sensed_motor_position[5]),
             sensor_power_data[5].voltage,
             sensor_power_data[5].current,
             sensor_power_data[5].power,
             analog_filter_get_value(&sensed_motor_position[6]),
             sensor_power_data[6].voltage,
             sensor_power_data[6].current,
             sensor_power_data[6].power,
             analog_filter_get_value(&sensed_motor_position[7]),
             sensor_power_data[7].voltage,
             sensor_power_data[7].current,
             sensor_power_data[7].power);

    send_to_controller(motor_message);


    // Make a second buffer for the board's data. We can't do all of this in one message.
    char board_message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH];
    memset(&board_message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

    snprintf(board_message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH,
                "BSENSE\tTEMP %.2f\tVBUS %.3f %.3f %.3f\tMP_IN %.3f %.3f %.3f\t3V3 %.3f %.3f %.3f\t5V %.3f %.3f %.3f",
                board_temperature,
                sensor_power_data[VBUS_SENSOR_SLOT].voltage,
                sensor_power_data[VBUS_SENSOR_SLOT].current,
                sensor_power_data[VBUS_SENSOR_SLOT].power,
                sensor_power_data[INCOMING_MOTOR_POWER_SENSOR_SLOT].voltage,
                sensor_power_data[INCOMING_MOTOR_POWER_SENSOR_SLOT].current,
                sensor_power_data[INCOMING_MOTOR_POWER_SENSOR_SLOT].power,
                sensor_power_data[V3v3_SENSOR_SLOT].voltage,
                sensor_power_data[V3v3_SENSOR_SLOT].current,
                sensor_power_data[V3v3_SENSOR_SLOT].power,
                sensor_power_data[V5_SENSOR_SLOT].voltage,
                sensor_power_data[V5_SENSOR_SLOT].current,
                sensor_power_data[V5_SENSOR_SLOT].power);

    send_to_controller(board_message);

    // Log some debug information, too
    debug("sensors: chassis: %.2fF, motor 0: pos: %u, %.2fV %.2fA %.2fW",
          board_temperature,
          analog_filter_get_value(&sensed_motor_position[0]),
          sensor_power_data[0].voltage,
          sensor_power_data[0].current,
          sensor_power_data[0].power);

    debug("board power: VBUS: %.3fA @ %.2fV, Incoming Motor: %.3fA @ %.2fV, 3v3: %.3fA @ %.2fV, 5v: %.3fA @ %.2fV",
          sensor_power_data[VBUS_SENSOR_SLOT].current,
          sensor_power_data[VBUS_SENSOR_SLOT].voltage,
          sensor_power_data[INCOMING_MOTOR_POWER_SENSOR_SLOT].current,
          sensor_power_data[INCOMING_MOTOR_POWER_SENSOR_SLOT].voltage,
          sensor_power_data[V3v3_SENSOR_SLOT].current,
          sensor_power_data[V3v3_SENSOR_SLOT].voltage,
          sensor_power_data[V5_SENSOR_SLOT].current,
          sensor_power_data[V5_SENSOR_SLOT].voltage);
}
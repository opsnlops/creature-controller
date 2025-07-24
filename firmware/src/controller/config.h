
#pragma once

#include <stdint.h>

#include "types.h"

/**
 * Main configuration for the controller
 */

#define INIT_REQUEST_TIME_MS 1000

// Are we debugging the ADC?
#define DEBUG_ADC 0

// We always want to be at 50Hz
#define SERVO_FREQUENCY 50

// Configure the watchdog timer
#define WATCHDOG_TIMEOUT_MS 5000
#define PWM_WRAPS_PER_WATCHDOG_UPDATE 100

// Light to flash when commands are being received
// #define CDC_ACTIVE_PIN                      17

// The most servos we can control
#define MAX_NUMBER_OF_SERVOS 8

// The number of steppers we can control
#define MAX_NUMBER_OF_STEPPERS 8

// Devices
#define POWER_PIN 28

/*
 * EEPROM Config
 */

#ifdef CC_VER3

#define USE_EEPROM 1

// Define the EEPROM page size (check the EEPROM's datasheet)
#define EEPROM_PAGE_SIZE 64

#define EEPROM_SDA_PIN 4
#define EEPROM_SCL_PIN 5
#define EEPROM_I2C_BUS i2c0
#define EEPROM_I2C_ADDR 0x50

// The NeoPixel status lights
#define STATUS_LIGHTS_TIME_MS 20
#define STATUS_LIGHTS_PIO pio1

#define STATUS_LIGHTS_LOGIC_BOARD_PIN 30
#define STATUS_LIGHTS_LOGIC_BOARD_IS_RGBW false

// Max brightness of the lights on the servo modules. Max is 255.
#define STATUS_LIGHTS_SERVOS_BRIGHTNESS 64

#define STATUS_LIGHTS_SERVOS_PIN 31
#define STATUS_LIGHTS_SERVOS_IS_RGBW false

#define USB_MOUNTED_LED_PIN 32

#define SENSOR_I2C_TIMER_TIME_MS 200

#endif

#ifdef CC_VER2

#define USE_EEPROM 0

// Define the EEPROM page size (check the EEPROM's datasheet)
#define EEPROM_PAGE_SIZE 64

#define EEPROM_SDA_PIN 2
#define EEPROM_SCL_PIN 3
#define EEPROM_I2C_BUS i2c1
#define EEPROM_I2C_ADDR 0x50

// The NeoPixel status lights
#define STATUS_LIGHTS_TIME_MS 20
#define STATUS_LIGHTS_PIO pio1

#define STATUS_LIGHTS_LOGIC_BOARD_PIN 17
#define STATUS_LIGHTS_LOGIC_BOARD_IS_RGBW false

// Max brightness of the lights on the servo modules. Max is 255.
#define STATUS_LIGHTS_SERVOS_BRIGHTNESS 64

#define STATUS_LIGHTS_SERVOS_PIN 14
#define STATUS_LIGHTS_SERVOS_IS_RGBW false

#define USB_MOUNTED_LED_PIN 15

#endif

// How many frames do we have to go before we decide there's no IO
#define STATUS_LIGHTS_IO_RESPONSIVENESS 25

// How many frames should we wait to turn off a motor's light?
#define STATUS_LIGHTS_MOTOR_OFF_FRAMES 100

#define STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS 0.1
#define STATUS_LIGHTS_RUNNING_BRIGHTNESS 0.1
#define STATUS_LIGHTS_RUNNING_FRAME_CHANGE 100

#define IO_LIGHT_COLOR_CYCLE_SPEED 2.3

/*
 * Steppers are currently totally disabled. Don't turn these back on without a
 * lot of thought, because I stole the pins for SPI.
 */

// Stepper
// #define STEPPER_LOOP_PERIOD_IN_US   1000        // The A3967 wants 1us pluses
// at a min
//
// #define STEPPER_MUX_BITS            3
// #define STEPPER_STEP_PIN            26
// #define STEPPER_DIR_PIN             27
// #define STEPPER_MS1_PIN             16
// #define STEPPER_MS2_PIN             17
// #define STEPPER_A0_PIN              18
// #define STEPPER_A1_PIN              19
// #define STEPPER_A2_PIN              20
// #define STEPPER_LATCH_PIN           21
// #define STEPPER_SLEEP_PIN           4
// #define STEPPER_END_S_LOW_PIN       14
// #define STEPPER_END_S_HIGH_PIN      15
// #define STEPPER_FAULT_PIN           13

/*
 * Microstepping configuration
 *
 * Look at the datasheet for the stepper driver currently in use to
 * know how to set this!
 *
 */
// #define STEPPER_MICROSTEP_MAX       8           // "8" means 1/8th step
// #define STEPPER_SPEED_0_MICROSTEPS  8           // At full speed, each step
// is 8 microsteps #define STEPPER_SPEED_1_MICROSTEPS  4           // ...4
// microsteps #define STEPPER_SPEED_2_MICROSTEPS  2           // ...2 microsteps
// #define STEPPER_SPEED_3_MICROSTEPS  1

#define USE_STEPPERS 0

/*
 * Logging Config
 */
#define DEFAULT_LOGGING_LEVEL LOG_LEVEL_DEBUG
#define LOGGING_QUEUE_LENGTH 100
#define LOGGING_MESSAGE_MAX_LENGTH 256
#define LOGGING_LOG_VIA_PRINTF                                                 \
    1 // Add a printf() in the logger. Useful when a debugger is attached

/*
 * Message Processor Config
 */
#define INCOMING_MESSAGE_QUEUE_LENGTH 5
#define INCOMING_MESSAGE_MAX_LENGTH 128

#define OUTGOING_MESSAGE_QUEUE_LENGTH 15
#define OUTGOING_MESSAGE_MAX_LENGTH 255

/*
 * USB Serial Config
 */
#define USB_SERIAL_INCOMING_QUEUE_LENGTH 5
#define USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH 128

#define USB_SERIAL_OUTGOING_QUEUE_LENGTH 15
#define USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH 255

/*
 * UART Serial Config
 */
#define UART_SERIAL_INCOMING_QUEUE_LENGTH 5
#define UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH 128

#define UART_SERIAL_OUTGOING_QUEUE_LENGTH 15
#define UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH 255

#define UART_DEVICE_NAME uart1
#define UART_RX_PIN 5
#define UART_TX_PIN 4
#define UART_BAUD_RATE 115200

// Used by the controller to signal that we need to reset
#define CONTROLLER_RESET_PIN 22
#define CONTROLLER_RESET_SIGNAL_PERIOD_MS 250

//
// Should we use the sensors?
//
//  If the sensors are enabled, but not connected, there's going to be an
//  assertion that gets thrown. This is entirely on purpose; if a board is
//  supposed to have sensors, but doesn't, we want to know about it.
//
//  If sensors are disabled, a warning will be generated at startup, and the
//  binary will be marked as not having sensors enabled. (so it can be read in
//  picotool)
//
#define USE_SENSORS 1

/*
 * I2C Config
 */
#ifdef CC_VER2
#define SENSORS_I2C_BUS i2c1
#define SENSORS_I2C_FREQ 400000
#define SENSORS_I2C_SDA_PIN 2
#define SENSORS_I2C_SCL_PIN 3

#define SENSORS_SPI_BUS spi0
#define SENSORS_SPI_FREQ (1000 * 750)
#define SENSORS_SPI_SCK_PIN 18
#define SENSORS_SPI_TX_PIN 19
#define SENSORS_SPI_RX_PIN 20
#define SENSORS_SPI_CS_PIN 21

// The smaller the number, the more often we log
#define SENSORS_SPI_LOG_CYCLES 2048

#define SENSOR_I2C_TIMER_TIME_MS 200
#define SENSOR_SPI_TIMER_TIME_MS 50

#endif

#ifdef CC_VER3

#define SENSORS_I2C_BUS i2c0
#define SENSORS_I2C_FREQ 400000
#define SENSORS_I2C_SDA_PIN 4
#define SENSORS_I2C_SCL_PIN 5

#define I2C_DEVICE_MCP9808 0x18
#define I2C_DEVICE_MCP9808_PRODUCT_ID                                          \
    0x400 // This is used to make sure we're talking to the right device

#define I2C_PAC1954_SENSOR_COUNT 3
#define I2C_PAC1954_PRODUCT_ID                                                 \
    0x7B // This is used to make sure we're talking to the right device

#define I2C_BOARD_PAC1954 0x10
#define I2C_BOARD_PAC1954_SENSOR_COUNT 3

#define VBUS_SENSOR_SLOT 0
#define INCOMING_MOTOR_POWER_SENSOR_SLOT 1
#define V3v3_SENSOR_SLOT 2

#endif

#ifdef CC_VER2

// Various I2C devices
#define I2C_DEVICE_MCP9808 0x18
#define I2C_DEVICE_MCP9808_PRODUCT_ID                                          \
    0x400 // This is used to make sure we're talking to the right device

#define I2C_PAC1954_SENSOR_COUNT 12
#define I2C_PAC1954_PRODUCT_ID                                                 \
    0x7B // This is used to make sure we're talking to the right device

#define I2C_MOTOR0_PAC1954 0x10
#define I2C_MOTOR0_PAC1954_SENSOR_COUNT 4

#define I2C_MOTOR1_PAC1954 0x11
#define I2C_MOTOR1_PAC1954_SENSOR_COUNT 4

#define I2C_BOARD_PAC1954 0x12
#define I2C_BOARD_PAC1954_SENSOR_COUNT 4

#define V5_SENSOR_SLOT 8
#define VBUS_SENSOR_SLOT 9
#define INCOMING_MOTOR_POWER_SENSOR_SLOT 10
#define V3v3_SENSOR_SLOT 11

#endif

/**
 * Analog Read Filter
 *
 * From the code:
 *
 *    SnapMultiplier is a value from 0 to 1 that controls the amount of easing.
 * Increase this to lessen the amount of easing (such as 0.1) and make the
 * responsive values more responsive, but doing so may cause more noise to seep
 * through when sleep is not enabled.
 */
#define ANALOG_READ_FILTER_SNAP_VALUE 0.1

/**
 * How much change do we need on the analog input to wake up the reader?
 */
#define ANALOG_READ_FILTER_ACTIVITY_THRESHOLD 20.0

/**
 * Should we use the edge snap feature?
 */
#define ANALOG_READ_FILTER_EDGE_SNAP_ENABLE 1

/*
 * Servo <-> GPIO Pin Mappings
 */
#ifdef CC_VER2
#define SERVO_0_GPIO_PIN 6  // Pin 9,  PWM  3A
#define SERVO_1_GPIO_PIN 7  // Pin 10, PWM  3B
#define SERVO_2_GPIO_PIN 8  // Pin 11, PWM  4A
#define SERVO_3_GPIO_PIN 9  // Pin 12, PWM  4B
#define SERVO_4_GPIO_PIN 10 // Pin 14, PWM  5A
#define SERVO_5_GPIO_PIN 11 // Pin 15, PWM  5B
#define SERVO_6_GPIO_PIN 12 // Pin 16, PWM  6A
#define SERVO_7_GPIO_PIN 13 // Pin 17, PWM  6B

// V2 doesn't have power control pins for the servos, so we just set them to 0
#define SERVO_0_POWER_PIN 0
#define SERVO_1_POWER_PIN 0
#define SERVO_2_POWER_PIN 0
#define SERVO_3_POWER_PIN 0
#define SERVO_4_POWER_PIN 0
#define SERVO_5_POWER_PIN 0
#define SERVO_6_POWER_PIN 0
#define SERVO_7_POWER_PIN 0

#endif
#ifdef CC_VER3
#define SERVO_0_GPIO_PIN 13
#define SERVO_1_GPIO_PIN 12
#define SERVO_2_GPIO_PIN 11
#define SERVO_3_GPIO_PIN 10
#define SERVO_4_GPIO_PIN 9
#define SERVO_5_GPIO_PIN 8
#define SERVO_6_GPIO_PIN 7
#define SERVO_7_GPIO_PIN 6

#define SERVO_0_POWER_PIN 21
#define SERVO_1_POWER_PIN 20
#define SERVO_2_POWER_PIN 19
#define SERVO_3_POWER_PIN 18
#define SERVO_4_POWER_PIN 17
#define SERVO_5_POWER_PIN 16
#define SERVO_6_POWER_PIN 15
#define SERVO_7_POWER_PIN 14

#endif

#define DEBUG_CREATURE_POSITIONING 0

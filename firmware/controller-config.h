
#pragma once

#include <stdint.h>

/**
* Main configuration for the controller
*/


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Just because it's funny
#define EVER ;;


/**
 * @brief Firmware Version
 *
 * This is the version that's sent to the controller. The controller is expected to know what
 * do to with this version.
 */
#define FIRMWARE_VERSION                    1
#define INIT_REQUEST_TIME_MS                1000


// Light to flash when commands are being received
#define CDC_ACTIVE_PIN                      17

// The most servos we can control
#define MAX_NUMBER_OF_SERVOS                8

// The number of steppers we can control
#define MAX_NUMBER_OF_STEPPERS              8

// Devices
#define E_STOP_PIN                          28

// Use the status lights?
#define USE_STATUS_LIGHTS                   1

// The NeoPixel status lights
#define STATUS_LIGHTS_TIME_MS               20
#define STATUS_LIGHTS_PIO                   pio1

#define STATUS_LIGHTS_LOGIC_BOARD_PIN       5
#define STATUS_LIGHTS_LOGIC_BOARD_IS_RGBW   false

// Max brightness of the lights on the servo modules. Max is 255.
#define STATUS_LIGHTS_SERVO_BRIGHTNESS      64

#define STATUS_LIGHTS_MOD_A_PIN             14
#define STATUS_LIGHTS_MOD_A_IS_RGBW         false

#define STATUS_LIGHTS_MOD_B_PIN             15
#define STATUS_LIGHTS_MOD_B_IS_RGBW         false

#define STATUS_LIGHTS_MOD_C_PIN             16
#define STATUS_LIGHTS_MOD_C_IS_RGBW         false

// How many frames do we have to go before we decide there's no IO
#define STATUS_LIGHTS_IO_RESPONSIVENESS     10

// How many frames should we wait to turn off a motor's light?
#define STATUS_LIGHTS_MOTOR_OFF_FRAMES      100

#define STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS 0.1
#define STATUS_LIGHTS_RUNNING_BRIGHTNESS    0.1
#define STATUS_LIGHTS_RUNNING_FRAME_CHANGE  100



// Stepper
#define STEPPER_LOOP_PERIOD_IN_US   1000        // The A3967 wants 1us pluses at a min

#define STEPPER_MUX_BITS            3
#define STEPPER_STEP_PIN            26
#define STEPPER_DIR_PIN             27
#define STEPPER_MS1_PIN             16
#define STEPPER_MS2_PIN             17
#define STEPPER_A0_PIN              18
#define STEPPER_A1_PIN              19
#define STEPPER_A2_PIN              20
#define STEPPER_LATCH_PIN           21
#define STEPPER_SLEEP_PIN           4
#define STEPPER_END_S_LOW_PIN       14
#define STEPPER_END_S_HIGH_PIN      15
#define STEPPER_FAULT_PIN           13

/*
 * Microstepping configuration
 *
 * Look at the datasheet for the stepper driver currently in use to
 * know how to set this!
 *
 */
#define STEPPER_MICROSTEP_MAX       8           // "8" means 1/8th step
#define STEPPER_SPEED_0_MICROSTEPS  8           // At full speed, each step is 8 microsteps
#define STEPPER_SPEED_1_MICROSTEPS  4           //                          ...4 microsteps
#define STEPPER_SPEED_2_MICROSTEPS  2           //                          ...2 microsteps
#define STEPPER_SPEED_3_MICROSTEPS  1

#define USE_STEPPERS                0




/*
 * Logging Config
 */
#define LOGGING_LEVEL               LOG_LEVEL_DEBUG
#define LOGGING_QUEUE_LENGTH        100
#define LOGGING_MESSAGE_MAX_LENGTH  256


/*
 * USB Serial Config
 */
#define USB_SERIAL_INCOMING_QUEUE_LENGTH        5
#define USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH  512

#define USB_SERIAL_OUTGOING_QUEUE_LENGTH        5
#define USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH  128


/*
 * Servo <-> GPIO Pin Mappings
 */
#define SERVO_0_GPIO_PIN            6               // Pin 9,  PMW  3A
#define SERVO_1_GPIO_PIN            7               // Pin 10, PWM  3B
#define SERVO_2_GPIO_PIN            8               // Pin 11, PWM  4A
#define SERVO_3_GPIO_PIN            9               // Pin 12, PWM  4B
#define SERVO_4_GPIO_PIN            10              // Pin 14, PWM  5A
#define SERVO_5_GPIO_PIN            11              // Pin 15, PWM  5B
#define SERVO_6_GPIO_PIN            12              // Pin 16, PWM  6A
#define SERVO_7_GPIO_PIN            13              // Pin 17, PWM  6B

#define DEBUG_CREATURE_POSITIONING  0

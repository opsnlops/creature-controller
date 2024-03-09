
#pragma once

#include <cstdint>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;


/**
* Main configuration for the controller
*/

// Just because it's funny
#define EVER ;;

#define BAUD_RATE                           B115200

// The most servos we can control
#define MAX_NUMBER_OF_SERVOS                8

// The number of steppers we can control
#define MAX_NUMBER_OF_STEPPERS              8

// Logging levels
#define CONTROLLER_LOG_LEVEL                "spdlog::level::debug"
#define RP2040_LOG_LEVEL                    "spdlog::level::trace"

// The version of the firmware we are expecting
#define FIRMWARE_VERSION                    3

// GPIO
#define GPIO_DEVICE                         "/dev/gpiomem"
#define FIRMWARE_RESET_PIN                  26

// These are from the server. They aren't used here, but just to note
// which ones are used over there.
#define SERVER_RUNNING_GPIO_PIN             17
#define PLAYING_ANIMATION_GPIO_PIN          27
#define PLAYING_SOUND_GPIO_PIN              22
#define RECEIVING_STREAM_FRAMES_GPIO_PIN    23
#define SENDING_DMX_GPIO_PIN                24
#define HEARTBEAT_GPIO_PIN                  25


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

#define USE_STEPPERS                1


#define DEFAULT_NETWORK_DEVICE_NAME   "eth0"

/**
 * These allow more than one creature to be controlled on the same
 * DMX universe!
 *
 * Everything is defined as an offset from the base. The values are
 * DMX_BASE_CHANNEL + the offset for the desired value.
 */
#define DMX_E_STOP_CHANNEL_OFFSET   0
#define DMX_NUMBER_OF_CHANNELS      13


/*
 * Position bounds
 */
#define MIN_POSITION                0
#define MAX_POSITION                1023
#define DEFAULT_POSITION            512

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
#define DEBUG_MESSAGE_PROCESSING    0

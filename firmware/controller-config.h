
#pragma once

/**
* Main configuration for the controller
*/

#define CREATURE_NAME                       "Beaky"


// Just because it's funny
#define EVER ;;

#define CDC_ACTIVE_PIN                      17

// The most servos we can control
#define MAX_NUMBER_OF_SERVOS                8

// The number of steppers we can control
#define MAX_NUMBER_OF_STEPPERS              8

// Devices
#define DMX_PIO                             pio0
#define DMX_INVERTED                        false
#define E_STOP_PIN                          28
#define DMX_GPIO_PIN                        22

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

#define STATUS_LIGHTS_DMX_STATUS_BRIGHTNESS 30
#define STATUS_LIGHTS_RUNNING_BRIGHTNESS    35
#define STATUS_LIGHTS_RUNNING_FRAME_CHANGE  20



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
 * Display Stuff
 */
#define DISPLAY_ENABLED             0
#define DISPLAY_I2C_BAUD_RATE       1000000
#define DISPLAY_I2C_CONTROLLER      i2c1
#define DISPLAY_I2C_DEVICE_ADDRESS  0x3C
#define DISPLAY_SDA_PIN             2
#define DISPLAY_SCL_PIN             3

// Update every 33ms (roughly 30Hz)
#define DISPLAY_UPDATE_TIME_MS      33
#define DISPLAY_BUFFER_SIZE         256     // This is per line
#define DISPLAY_NUMBER_OF_LINES     4


/*
 * Logging Config
 */
#define LOGGING_LEVEL               LOG_LEVEL_DEBUG
#define LOGGING_QUEUE_LENGTH        100
#define LOGGING_MESSAGE_MAX_LENGTH  256

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

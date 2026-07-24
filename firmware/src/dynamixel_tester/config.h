
#pragma once

/**
 * @file config.h
 * @brief Configuration for the Dynamixel Servo Tester
 */

// Dynamixel data line - any GPIO works with PIO
#define DXL_DATA_PIN 22

// Which PIO instance to use (pio0 has all 4 SMs free)
#define DXL_PIO_INSTANCE pio0

// Default baud rate for Dynamixel communication
#define DXL_DEFAULT_BAUD_RATE 1000000

// Shell command buffers
#define INCOMING_REQUEST_BUFFER_SIZE 128
#define OUTGOING_RESPONSE_BUFFER_SIZE 256

// Status LEDs
//
// Three activity indicators: USB CDC mounted, incoming data, outgoing data.
// The count is hardware-independent; only the way they are driven changes.
#define STATUS_LED_COUNT 3

#if defined(CC_VER4)

// HW4 boards have no discrete indicator LEDs. Mirror the three indicators onto
// the first three pixels of the logic-board WS2812 chain (GPIO 30 - the same
// output the controller firmware uses). Dynamixel owns pio0 here, so drive the
// status chain from pio1. Pin 30 is inside the default 0-31 PIO window, so no
// pio_set_gpio_base() adjustment is needed.
#define STATUS_LED_USE_WS2812 1
#define STATUS_LED_WS2812_PIN 30
#define STATUS_LED_WS2812_PIO pio1
#define STATUS_LED_WS2812_IS_RGBW false
#define STATUS_LED_WS2812_FREQ 800000
#define STATUS_LED_BRIGHTNESS 64

// "Board is alive" light show
//
// The tester doesn't drive real servos, so the servo-module and Dynamixel LED
// chains sit dark. Animate them with a scrolling rainbow as a FreeRTOS liveness
// indicator - the modern equivalent of blinking the on-board LED. Driven from
// pio2 (the RP2350's third PIO), which is untouched by the DXL bus (pio0) and
// the status indicators (pio1). The PIO GPIO base is moved to 16 so GPIO 34
// (outside the default 0-31 window) is reachable; GPIO 31 stays in range too.
#define ALIVE_DISPLAY_PIO pio2
#define ALIVE_DISPLAY_PIO_GPIO_BASE 16
#define ALIVE_DISPLAY_FREQ 800000
#define ALIVE_DISPLAY_BRIGHTNESS 48

// Servo-module LED chain (GPIO 31)
#define ALIVE_DISPLAY_SERVO_PIN 31
#define ALIVE_DISPLAY_SERVO_COUNT 8
#define ALIVE_DISPLAY_SERVO_IS_RGBW false

// Dynamixel status LED chain (GPIO 34)
#define ALIVE_DISPLAY_DXL_PIN 34
#define ALIVE_DISPLAY_DXL_COUNT 8
#define ALIVE_DISPLAY_DXL_IS_RGBW false

// Animation timing and look
#define ALIVE_DISPLAY_FRAME_MS 33          // ~30 fps
#define ALIVE_DISPLAY_HUE_STEP_PER_FRAME 3 // How fast the rainbow scrolls
#define ALIVE_DISPLAY_HUE_SPREAD 24        // Hue delta between adjacent pixels
#define ALIVE_DISPLAY_CHAIN_HUE_OFFSET 128 // Hue offset between the two chains
#define ALIVE_DISPLAY_TASK_STACK_SIZE 512
#define ALIVE_DISPLAY_TASK_PRIORITY 1      // Lowest useful priority; never blocks the bus

#else

// Older boards / generic dev boards: three discrete GPIO LEDs.
#define STATUS_LED_USE_WS2812 0
#define CDC_MOUNTED_LED_PIN 16
#define INCOMING_LED_PIN 17
#define OUTGOING_LED_PIN 18

#endif

// Watchdog configuration
#define WATCHDOG_TIMER_PERIOD_MS 1000
#define WATCHDOG_TIMEOUT_MS 5000

/*
 * Logging Config now lives in src/logging/logging_config.h (the single source
 * of truth, included via logging.h), so it is no longer duplicated here.
 */

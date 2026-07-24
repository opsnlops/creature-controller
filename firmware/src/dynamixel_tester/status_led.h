#pragma once

/**
 * @file status_led.h
 * @brief Status indicator LEDs for the Dynamixel Servo Tester
 *
 * The tester shows three activity indicators: USB CDC mounted, incoming data,
 * and outgoing data.
 *
 * On HW4 boards there are no discrete indicator LEDs, so these three signals
 * are mirrored onto the first three pixels of the logic-board WS2812 chain
 * (mounted -> green, incoming -> blue, outgoing -> magenta). On older boards
 * (and generic dev boards) three discrete GPIO LEDs are driven instead. The
 * interface below is identical either way, so callers do not care which board
 * they are running on.
 */

#include <stdbool.h>

#include "types.h"

// The three activity indicators. On the WS2812 build the enum value doubles as
// the pixel index along the chain, so the order here defines the chain order.
typedef enum {
    STATUS_LED_MOUNTED = 0,
    STATUS_LED_INCOMING,
    STATUS_LED_OUTGOING,
} status_led_t;

/**
 * @brief Initialize the status LED hardware.
 *
 * Sets up either the WS2812 PIO state machine (HW4) or the discrete GPIO LED
 * pins (older boards) and leaves all indicators off.
 */
void status_led_init(void);

/**
 * @brief Turn a single status indicator on or off.
 *
 * @param which The indicator to change
 * @param on    true to light it, false to clear it
 */
void status_led_set(status_led_t which, bool on);

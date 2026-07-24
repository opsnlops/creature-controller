/**
 * @file status_led.c
 * @brief Status indicator LED implementation for the Dynamixel Servo Tester
 *
 * Two back ends are compiled conditionally: a WS2812 chain on HW4 boards, and
 * discrete GPIO LEDs on everything else. Which one is active is selected by
 * STATUS_LED_USE_WS2812 in config.h.
 */

#include "status_led.h"

#include "config.h"

#if STATUS_LED_USE_WS2812

#include <hardware/pio.h>

#include "ws2812.pio.h"

#include "logging/logging.h"

// "On" color for each indicator, in WS2812 GRB order (0x00GGRRBB), scaled to
// STATUS_LED_BRIGHTNESS. Indexed by status_led_t, so the order matches the
// pixel order along the chain:
//   mounted  -> green
//   incoming -> blue
//   outgoing -> magenta (red + blue)
static const u32 on_color[STATUS_LED_COUNT] = {
    [STATUS_LED_MOUNTED] = (u32)STATUS_LED_BRIGHTNESS << 16,
    [STATUS_LED_INCOMING] = (u32)STATUS_LED_BRIGHTNESS,
    [STATUS_LED_OUTGOING] = ((u32)STATUS_LED_BRIGHTNESS << 8) | (u32)STATUS_LED_BRIGHTNESS,
};

// Current GRB value for each pixel; the whole chain is re-clocked on any change.
static u32 pixel_state[STATUS_LED_COUNT];
static u8 status_led_sm;

// Re-send every pixel in order. WS2812 chains are addressed positionally, so a
// single indicator change still requires clocking out all pixels.
static void refresh(void) {
    for (u8 i = 0; i < STATUS_LED_COUNT; i++) {
        pio_sm_put_blocking(STATUS_LED_WS2812_PIO, status_led_sm, pixel_state[i] << 8u);
    }
}

void status_led_init(void) {
    uint offset = pio_add_program(STATUS_LED_WS2812_PIO, &ws2812_program);
    status_led_sm = pio_claim_unused_sm(STATUS_LED_WS2812_PIO, true);
    debug("status LED state machine: %u", status_led_sm);

    ws2812_program_init(STATUS_LED_WS2812_PIO, status_led_sm, offset, STATUS_LED_WS2812_PIN, STATUS_LED_WS2812_FREQ,
                        STATUS_LED_WS2812_IS_RGBW);

    for (u8 i = 0; i < STATUS_LED_COUNT; i++) {
        pixel_state[i] = 0u;
    }
    refresh();
}

void status_led_set(status_led_t which, bool on) {
    if ((u8)which >= STATUS_LED_COUNT) {
        return;
    }
    pixel_state[which] = on ? on_color[which] : 0u;
    refresh();
}

#else // Discrete GPIO LEDs (older boards / generic dev boards)

#include <hardware/gpio.h>

static u8 led_pin(status_led_t which) {
    switch (which) {
    case STATUS_LED_INCOMING:
        return INCOMING_LED_PIN;
    case STATUS_LED_OUTGOING:
        return OUTGOING_LED_PIN;
    case STATUS_LED_MOUNTED:
    default:
        return CDC_MOUNTED_LED_PIN;
    }
}

void status_led_init(void) {
    const u8 pins[STATUS_LED_COUNT] = {CDC_MOUNTED_LED_PIN, INCOMING_LED_PIN, OUTGOING_LED_PIN};
    for (u8 i = 0; i < STATUS_LED_COUNT; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], false);
    }
}

void status_led_set(status_led_t which, bool on) { gpio_put(led_pin(which), on); }

#endif

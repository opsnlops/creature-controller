/**
 * @file alive_display.c
 * @brief "Board is alive" LED light show for the Dynamixel Servo Tester
 *
 * Scrolls a rainbow across the otherwise-idle servo-module and Dynamixel LED
 * chains to show the FreeRTOS scheduler is running. HW4 only; on older boards
 * these chains don't exist, so the whole thing compiles to no-ops.
 */

#include "alive_display.h"

#include "config.h"

#if defined(CC_VER4)

#include <FreeRTOS.h>
#include <task.h>

#include <hardware/pio.h>

#include "ws2812.pio.h"

#include "logging/logging.h"

#include "types.h"

static u8 servo_sm;
static u8 dxl_sm;

portTASK_FUNCTION_PROTO(alive_display_task, pvParameters);

// Classic color wheel: map a hue position (0-255) around the spectrum and pack
// it into WS2812 GRB order (0x00GGRRBB), scaled to ALIVE_DISPLAY_BRIGHTNESS.
static u32 wheel(u8 pos) {
    u8 r, g, b;
    if (pos < 85) {
        r = 255 - pos * 3;
        g = 0;
        b = pos * 3;
    } else if (pos < 170) {
        pos -= 85;
        r = 0;
        g = pos * 3;
        b = 255 - pos * 3;
    } else {
        pos -= 170;
        r = pos * 3;
        g = 255 - pos * 3;
        b = 0;
    }

    r = (u8)((u16)r * ALIVE_DISPLAY_BRIGHTNESS / 255);
    g = (u8)((u16)g * ALIVE_DISPLAY_BRIGHTNESS / 255);
    b = (u8)((u16)b * ALIVE_DISPLAY_BRIGHTNESS / 255);

    return ((u32)g << 16) | ((u32)r << 8) | (u32)b;
}

// Clock a rainbow across one chain, starting from base_hue and stepping the hue
// by ALIVE_DISPLAY_HUE_SPREAD per pixel.
static void render_chain(u8 sm, u8 count, u8 base_hue) {
    for (u8 i = 0; i < count; i++) {
        u8 pos = (u8)(base_hue + i * ALIVE_DISPLAY_HUE_SPREAD);
        pio_sm_put_blocking(ALIVE_DISPLAY_PIO, sm, wheel(pos) << 8u);
    }
}

static void blank_chain(u8 sm, u8 count) {
    for (u8 i = 0; i < count; i++) {
        pio_sm_put_blocking(ALIVE_DISPLAY_PIO, sm, 0u);
    }
}

void alive_display_init(void) {
    // GPIO 34 sits outside the default 0-31 PIO window, so shift this PIO's base
    // to 16 (covering 16-47) before loading any program. GPIO 31 is still in
    // range. This must happen before pio_add_program()/state-machine setup.
    pio_set_gpio_base(ALIVE_DISPLAY_PIO, ALIVE_DISPLAY_PIO_GPIO_BASE);

    uint offset = pio_add_program(ALIVE_DISPLAY_PIO, &ws2812_program);

    servo_sm = pio_claim_unused_sm(ALIVE_DISPLAY_PIO, true);
    ws2812_program_init(ALIVE_DISPLAY_PIO, servo_sm, offset, ALIVE_DISPLAY_SERVO_PIN, ALIVE_DISPLAY_FREQ,
                        ALIVE_DISPLAY_SERVO_IS_RGBW);

    dxl_sm = pio_claim_unused_sm(ALIVE_DISPLAY_PIO, true);
    ws2812_program_init(ALIVE_DISPLAY_PIO, dxl_sm, offset, ALIVE_DISPLAY_DXL_PIN, ALIVE_DISPLAY_FREQ,
                        ALIVE_DISPLAY_DXL_IS_RGBW);

    blank_chain(servo_sm, ALIVE_DISPLAY_SERVO_COUNT);
    blank_chain(dxl_sm, ALIVE_DISPLAY_DXL_COUNT);

    info("alive display initialized (servo SM %u, dxl SM %u)", servo_sm, dxl_sm);
}

void alive_display_start(void) {
    xTaskCreate(alive_display_task, "alive_display", ALIVE_DISPLAY_TASK_STACK_SIZE, NULL, ALIVE_DISPLAY_TASK_PRIORITY,
                NULL);
    debug("alive display task started");
}

portTASK_FUNCTION(alive_display_task, pvParameters) {
    (void)pvParameters;

    u8 phase = 0;
    for (EVER) {
        render_chain(servo_sm, ALIVE_DISPLAY_SERVO_COUNT, phase);
        // Offset the Dynamixel chain so the two chains don't look identical.
        render_chain(dxl_sm, ALIVE_DISPLAY_DXL_COUNT, (u8)(phase + ALIVE_DISPLAY_CHAIN_HUE_OFFSET));

        phase = (u8)(phase + ALIVE_DISPLAY_HUE_STEP_PER_FRAME);
        vTaskDelay(pdMS_TO_TICKS(ALIVE_DISPLAY_FRAME_MS));
    }
}

#else // Not HW4: these LED chains don't exist.

void alive_display_init(void) {}
void alive_display_start(void) {}

#endif

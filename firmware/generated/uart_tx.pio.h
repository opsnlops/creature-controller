// -------------------------------------------------- //
// This file is autogenerated by pioasm; do not edit! //
// -------------------------------------------------- //

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// ------- //
// uart_tx //
// ------- //

#define uart_tx_wrap_target 0
#define uart_tx_wrap 3
#define uart_tx_pio_version 1

static const uint16_t uart_tx_program_instructions[] = {
            //     .wrap_target
    0x9fa0, //  0: pull   block           side 1 [7] 
    0xf727, //  1: set    x, 7            side 0 [7] 
    0x6001, //  2: out    pins, 1                    
    0x0642, //  3: jmp    x--, 2                 [6] 
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program uart_tx_program = {
    .instructions = uart_tx_program_instructions,
    .length = 4,
    .origin = -1,
    .pio_version = 1,
#if PICO_PIO_VERSION > 0
    .used_gpio_ranges = 0x0
#endif
};

static inline pio_sm_config uart_tx_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + uart_tx_wrap_target, offset + uart_tx_wrap);
    sm_config_set_sideset(&c, 2, true, false);
    return c;
}

#include "hardware/clocks.h"
static inline void uart_tx_program_init(PIO pio, uint sm, uint offset, uint pin_tx, uint baud) {
    // Tell PIO to initially drive output-high on the selected pin, then map PIO
    // onto that pin with the IO muxes.
    pio_sm_set_pins_with_mask(pio, sm, 1u << pin_tx, 1u << pin_tx);
    pio_sm_set_pindirs_with_mask(pio, sm, 1u << pin_tx, 1u << pin_tx);
    pio_gpio_init(pio, pin_tx);
    pio_sm_config c = uart_tx_program_get_default_config(offset);
    // OUT shifts to right, no autopull
    sm_config_set_out_shift(&c, true, false, 32);
    // We are mapping both OUT and side-set to the same pin, because sometimes
    // we need to assert user data onto the pin (with OUT) and sometimes
    // assert constant values (start/stop bit)
    sm_config_set_out_pins(&c, pin_tx, 1);
    sm_config_set_sideset_pins(&c, pin_tx);
    // We only need TX, so get an 8-deep FIFO!
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    // SM transmits 1 bit per 8 execution cycles.
    float div = (float)clock_get_hz(clk_sys) / (8 * baud);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
static inline void uart_tx_program_putc(PIO pio, uint sm, char c) {
    pio_sm_put_blocking(pio, sm, (uint32_t)c);
}
static inline void uart_tx_program_puts(PIO pio, uint sm, const char *s) {
    while (*s)
        uart_tx_program_putc(pio, sm, *s++);
}

#endif


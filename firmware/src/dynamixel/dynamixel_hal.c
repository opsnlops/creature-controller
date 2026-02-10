
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/time.h"

#include "logging/logging.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#include "dynamixel_hal.h"
#include "dynamixel_registers.h"

struct dxl_hal_context {
    PIO pio;
    u32 sm_tx;
    u32 sm_rx;
    u32 offset_tx;
    u32 offset_rx;
    u32 data_pin;
    u32 baud_rate;
    u32 dma_chan;
    u8 rx_buffer[DXL_MAX_PACKET_SIZE];
};

/**
 * Wait for approximately one byte-time at the current baud rate.
 * This ensures the TX shift register has fully drained before switching to RX.
 */
static void wait_one_byte_time(u32 baud_rate) {
    // One byte at 8N1 = 10 bits. Add some margin.
    u32 us = (12 * 1000000) / baud_rate;
    if (us < 1)
        us = 1;
    busy_wait_us_32(us);
}

dxl_hal_context_t *dxl_hal_init(const dxl_hal_config_t *config) {
    dxl_hal_context_t *ctx = pvPortMalloc(sizeof(dxl_hal_context_t));
    if (ctx == NULL) {
        error("dynamixel HAL: failed to allocate context");
        return NULL;
    }
    memset(ctx, 0, sizeof(dxl_hal_context_t));

    ctx->pio = config->pio;
    ctx->data_pin = config->data_pin;
    ctx->baud_rate = config->baud_rate;

    // Load PIO programs
    if (!pio_can_add_program(ctx->pio, &uart_tx_program)) {
        error("dynamixel HAL: cannot add TX PIO program");
        vPortFree(ctx);
        return NULL;
    }
    ctx->offset_tx = pio_add_program(ctx->pio, &uart_tx_program);

    if (!pio_can_add_program(ctx->pio, &uart_rx_program)) {
        error("dynamixel HAL: cannot add RX PIO program");
        vPortFree(ctx);
        return NULL;
    }
    ctx->offset_rx = pio_add_program(ctx->pio, &uart_rx_program);

    // Claim state machines
    int sm_tx = pio_claim_unused_sm(ctx->pio, false);
    if (sm_tx < 0) {
        error("dynamixel HAL: no free TX state machine");
        vPortFree(ctx);
        return NULL;
    }
    ctx->sm_tx = (u32)sm_tx;

    int sm_rx = pio_claim_unused_sm(ctx->pio, false);
    if (sm_rx < 0) {
        error("dynamixel HAL: no free RX state machine");
        pio_sm_unclaim(ctx->pio, ctx->sm_tx);
        vPortFree(ctx);
        return NULL;
    }
    ctx->sm_rx = (u32)sm_rx;

    // Initialize both SMs on the same pin.
    // The init helpers configure and enable the SM, so we disable them immediately.
    // Crucially, we must clear the TX SM's pindir after init. PIO GPIO output enable
    // is the OR of all SMs' pindirs, so if TX leaves its pindir as output, the pin
    // can never become an input for RX. We manage pin directions explicitly in txrx.
    uart_tx_program_init(ctx->pio, ctx->sm_tx, ctx->offset_tx, ctx->data_pin, ctx->baud_rate);
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);

    uart_rx_program_init(ctx->pio, ctx->sm_rx, ctx->offset_rx, ctx->data_pin, ctx->baud_rate);
    pio_sm_set_enabled(ctx->pio, ctx->sm_rx, false);

    // Claim a DMA channel
    ctx->dma_chan = (u32)dma_claim_unused_channel(true);

    info("dynamixel HAL initialized: pin=%lu, baud=%lu, pio=%s, sm_tx=%lu, sm_rx=%lu, dma=%lu", ctx->data_pin,
         ctx->baud_rate, ctx->pio == pio0 ? "pio0" : "pio1", ctx->sm_tx, ctx->sm_rx, ctx->dma_chan);

    return ctx;
}

void dxl_hal_set_baud_rate(dxl_hal_context_t *ctx, u32 baud_rate) {
    ctx->baud_rate = baud_rate;

    float div = (float)clock_get_hz(clk_sys) / (8.0f * (float)baud_rate);

    pio_sm_set_clkdiv(ctx->pio, ctx->sm_tx, div);
    pio_sm_set_clkdiv(ctx->pio, ctx->sm_rx, div);

    info("dynamixel HAL: baud rate changed to %lu", baud_rate);
}

u32 dxl_hal_get_baud_rate(dxl_hal_context_t *ctx) { return ctx->baud_rate; }

void dxl_hal_flush_rx(dxl_hal_context_t *ctx) {
    // Abort any in-progress DMA
    dma_channel_abort(ctx->dma_chan);

    // Drain the RX FIFO
    while (!pio_sm_is_rx_fifo_empty(ctx->pio, ctx->sm_rx)) {
        (void)pio_sm_get(ctx->pio, ctx->sm_rx);
    }
}

dxl_result_t dxl_hal_txrx(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt, dxl_packet_t *rx_pkt, u32 timeout_ms) {
    // Broadcast packets get no response - just transmit
    if (tx_pkt->id == DXL_BROADCAST_ID) {
        return dxl_hal_tx(ctx, tx_pkt);
    }

    // Build wire packet
    u8 tx_buf[DXL_MAX_PACKET_SIZE];
    size_t tx_len = 0;

    dxl_result_t result = dxl_build_packet(tx_pkt, tx_buf, sizeof(tx_buf), &tx_len);
    if (result != DXL_OK) {
        return result;
    }

    // Step 1: Flush any stale RX data
    dxl_hal_flush_rx(ctx);

    // Step 2: Set TX SM pindir to output and enable TX
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 1u << ctx->data_pin, 1u << ctx->data_pin);
    pio_sm_restart(ctx->pio, ctx->sm_tx);
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, true);

    // Step 3: Write packet bytes via PIO
    for (size_t i = 0; i < tx_len; i++) {
        pio_sm_put_blocking(ctx->pio, ctx->sm_tx, (u32)tx_buf[i]);
    }

    // Step 4: Wait for TX FIFO to empty + shift register to drain
    while (!pio_sm_is_tx_fifo_empty(ctx->pio, ctx->sm_tx)) {
        tight_loop_contents();
    }
    wait_one_byte_time(ctx->baud_rate);

    // Step 5: Disable TX SM and release pin direction so RX can use it
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);

    // Step 6: Enable RX SM (pin is now input with pull-up)
    pio_sm_restart(ctx->pio, ctx->sm_rx);
    pio_sm_set_enabled(ctx->pio, ctx->sm_rx, true);

    // Step 7: Configure DMA for RX
    memset(ctx->rx_buffer, 0, DXL_MAX_PACKET_SIZE);

    dma_channel_config dma_cfg = dma_channel_get_default_config(ctx->dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_cfg, false); // read from fixed PIO FIFO addr
    channel_config_set_write_increment(&dma_cfg, true); // write to incrementing buffer
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(ctx->pio, ctx->sm_rx, false));

    // We read from the uppermost byte of the FIFO for 8-bit data (left-justified)
    volatile void *rx_fifo_addr = (volatile void *)((uintptr_t)&ctx->pio->rxf[ctx->sm_rx] + 3);

    dma_channel_configure(ctx->dma_chan, &dma_cfg,
                          ctx->rx_buffer,      // write address
                          rx_fifo_addr,        // read address (upper byte of RX FIFO)
                          DXL_MAX_PACKET_SIZE, // max transfer count
                          true                 // start immediately
    );

    // Step 8-9: Poll for received data
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    size_t bytes_received = 0;
    size_t expected_total = 0;

    while (!time_reached(deadline)) {
        // How many bytes have we received so far?
        u32 remaining = dma_channel_hw_addr(ctx->dma_chan)->transfer_count;
        bytes_received = DXL_MAX_PACKET_SIZE - remaining;

        // Once we have the length field (bytes 5-6, so need >= 7 bytes), calculate expected total
        if (bytes_received >= 7 && expected_total == 0) {
            u16 wire_length = (u16)ctx->rx_buffer[5] | ((u16)ctx->rx_buffer[6] << 8);
            expected_total = 7 + (size_t)wire_length;

            if (expected_total > DXL_MAX_PACKET_SIZE) {
                // Something is wrong, bail out
                break;
            }
        }

        // Check if we've received the full packet
        if (expected_total > 0 && bytes_received >= expected_total) {
            break;
        }

        // Brief yield to avoid hammering the CPU
        busy_wait_us_32(10);
    }

    // Step 10: Abort DMA and disable RX SM
    dma_channel_abort(ctx->dma_chan);
    pio_sm_set_enabled(ctx->pio, ctx->sm_rx, false);

    // Recheck final count
    u32 final_remaining = dma_channel_hw_addr(ctx->dma_chan)->transfer_count;
    bytes_received = DXL_MAX_PACKET_SIZE - final_remaining;

    if (bytes_received < 10) {
        // Minimum valid response is 10 bytes (for a status packet with no params)
        // But we could have gotten nothing at all (timeout)
        return DXL_TIMEOUT;
    }

    // Step 11: Parse the response
    return dxl_parse_packet(ctx->rx_buffer, bytes_received, rx_pkt);
}

dxl_result_t dxl_hal_tx(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt) {
    u8 tx_buf[DXL_MAX_PACKET_SIZE];
    size_t tx_len = 0;

    dxl_result_t result = dxl_build_packet(tx_pkt, tx_buf, sizeof(tx_buf), &tx_len);
    if (result != DXL_OK) {
        return result;
    }

    // Set TX SM pindir to output and enable
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 1u << ctx->data_pin, 1u << ctx->data_pin);
    pio_sm_restart(ctx->pio, ctx->sm_tx);
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, true);

    // Write packet bytes
    for (size_t i = 0; i < tx_len; i++) {
        pio_sm_put_blocking(ctx->pio, ctx->sm_tx, (u32)tx_buf[i]);
    }

    // Wait for TX to complete
    while (!pio_sm_is_tx_fifo_empty(ctx->pio, ctx->sm_tx)) {
        tight_loop_contents();
    }
    wait_one_byte_time(ctx->baud_rate);

    // Disable TX SM and release pin direction back to input
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);

    return DXL_OK;
}

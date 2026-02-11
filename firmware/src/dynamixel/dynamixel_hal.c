
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "pico/time.h"

#include "logging/logging.h"

#include "uart_rx.pio.h"
#include "uart_tx.pio.h"

#include "dynamixel_hal.h"
#include "dynamixel_registers.h"

/**
 * State shared between the calling task and the RX alarm callback
 * during dxl_hal_txrx_multi(). Fields are set by the caller before
 * the alarm starts; only last_seen_bytes and idle_count are written
 * by the ISR.
 */
struct dxl_alarm_state {
    volatile u32 last_seen_bytes;
    volatile u32 idle_count;
    u32 idle_limit;
    u32 rx_buf_size;
    u32 dma_chan;
    u32 alarm_period_us;
    SemaphoreHandle_t rx_sem;
};

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
    SemaphoreHandle_t rx_complete_sem;
    alarm_pool_t *alarm_pool;
    struct dxl_alarm_state alarm_state;

    u8 last_servo_error; // Protocol error byte from most recent servo response

    // Pre-allocated workspace for multi-servo operations (avoids per-frame heap allocation)
    u8 multi_rx_buf[DXL_MULTI_RX_BUF_SIZE];
    dxl_packet_t work_pkt;
    dxl_packet_t multi_rx_pkts[DXL_MAX_MULTI_RESPONSES];
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

/**
 * Compute a generous TX deadline based on baud rate.
 * Allows enough time to transmit a full max-size packet plus margin.
 */
static absolute_time_t make_tx_deadline(u32 baud_rate) {
    u32 tx_timeout_us = (DXL_MAX_PACKET_SIZE * 12 * 1000000ULL) / baud_rate + 5000;
    return make_timeout_time_us(tx_timeout_us);
}

/**
 * Write a byte to the PIO TX FIFO with a timeout guard.
 * Returns DXL_OK on success, DXL_TIMEOUT if the FIFO stays full past the deadline.
 */
static dxl_result_t pio_put_with_timeout(PIO pio, u32 sm, u32 data, absolute_time_t deadline) {
    while (pio_sm_is_tx_fifo_full(pio, sm)) {
        if (time_reached(deadline)) {
            return DXL_TIMEOUT;
        }
        tight_loop_contents();
    }
    pio_sm_put(pio, sm, data);
    return DXL_OK;
}

/**
 * Wait for TX FIFO to drain and shift register to finish, with timeout.
 * Returns DXL_OK on success, DXL_TIMEOUT if the FIFO doesn't empty in time.
 */
static dxl_result_t wait_tx_complete(PIO pio, u32 sm, u32 baud_rate, absolute_time_t deadline) {
    while (!pio_sm_is_tx_fifo_empty(pio, sm)) {
        if (time_reached(deadline)) {
            return DXL_TIMEOUT;
        }
        tight_loop_contents();
    }
    wait_one_byte_time(baud_rate);
    return DXL_OK;
}

/**
 * Repeating alarm callback for multi-response RX idle detection.
 *
 * Fires every alarm_period_us during the RX phase of dxl_hal_txrx_multi().
 * Compares DMA transfer_count against the last check to detect when the bus
 * has gone idle (no new bytes arriving). Once idle for idle_limit consecutive
 * checks, signals the binary semaphore so the calling task can wake up.
 *
 * Returns negative period to reschedule, or 0 to stop.
 */
static int64_t dxl_rxmulti_alarm_callback(alarm_id_t id, void *user_data) {
    (void)id;
    struct dxl_alarm_state *state = (struct dxl_alarm_state *)user_data;

    u32 remaining = dma_channel_hw_addr(state->dma_chan)->transfer_count;
    u32 bytes_received = state->rx_buf_size - remaining;

    if (bytes_received != state->last_seen_bytes) {
        state->last_seen_bytes = bytes_received;
        state->idle_count = 0;
    } else if (state->last_seen_bytes > 0) {
        state->idle_count++;
    }

    if (state->idle_count >= state->idle_limit) {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(state->rx_sem, &woken);
        portYIELD_FROM_ISR(woken);
        return 0;
    }

    return -(int64_t)state->alarm_period_us;
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
        goto fail_free_ctx;
    }
    ctx->offset_tx = pio_add_program(ctx->pio, &uart_tx_program);

    if (!pio_can_add_program(ctx->pio, &uart_rx_program)) {
        error("dynamixel HAL: cannot add RX PIO program");
        goto fail_remove_tx_program;
    }
    ctx->offset_rx = pio_add_program(ctx->pio, &uart_rx_program);

    // Claim state machines
    int sm_tx = pio_claim_unused_sm(ctx->pio, false);
    if (sm_tx < 0) {
        error("dynamixel HAL: no free TX state machine");
        goto fail_remove_rx_program;
    }
    ctx->sm_tx = (u32)sm_tx;

    int sm_rx = pio_claim_unused_sm(ctx->pio, false);
    if (sm_rx < 0) {
        error("dynamixel HAL: no free RX state machine");
        goto fail_unclaim_sm_tx;
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
    int dma = dma_claim_unused_channel(false);
    if (dma < 0) {
        error("dynamixel HAL: no free DMA channel");
        goto fail_unclaim_sm_rx;
    }
    ctx->dma_chan = (u32)dma;

    // Create binary semaphore for multi-response RX idle detection
    ctx->rx_complete_sem = xSemaphoreCreateBinary();
    if (ctx->rx_complete_sem == NULL) {
        error("dynamixel HAL: failed to create RX semaphore");
        goto fail_release_dma;
    }
    ctx->alarm_state.rx_sem = ctx->rx_complete_sem;

    // Create a dedicated alarm pool with IRQ priority safe for FreeRTOS API calls.
    // The default pool uses PICO_DEFAULT_IRQ_PRIORITY (0x80), which is higher priority
    // than configMAX_SYSCALL_INTERRUPT_PRIORITY (0xA0) and cannot call xSemaphoreGiveFromISR.
    ctx->alarm_pool = alarm_pool_create_with_unused_hardware_alarm(4);
    if (ctx->alarm_pool == NULL) {
        error("dynamixel HAL: failed to create alarm pool");
        goto fail_delete_sem;
    }
    uint alarm_num = alarm_pool_hardware_alarm_num(ctx->alarm_pool);
    uint alarm_irq = hardware_alarm_get_irq_num(alarm_num);
    irq_set_priority(alarm_irq, configMAX_SYSCALL_INTERRUPT_PRIORITY);

    info("dynamixel HAL initialized: pin=%lu, baud=%lu, pio=%s, sm_tx=%lu, sm_rx=%lu, dma=%lu", ctx->data_pin,
         ctx->baud_rate, ctx->pio == pio0 ? "pio0" : "pio1", ctx->sm_tx, ctx->sm_rx, ctx->dma_chan);

    return ctx;

fail_delete_sem:
    vSemaphoreDelete(ctx->rx_complete_sem);
fail_release_dma:
    dma_channel_unclaim(ctx->dma_chan);
fail_unclaim_sm_rx:
    pio_sm_unclaim(ctx->pio, ctx->sm_rx);
fail_unclaim_sm_tx:
    pio_sm_unclaim(ctx->pio, ctx->sm_tx);
fail_remove_rx_program:
    pio_remove_program(ctx->pio, &uart_rx_program, ctx->offset_rx);
fail_remove_tx_program:
    pio_remove_program(ctx->pio, &uart_tx_program, ctx->offset_tx);
fail_free_ctx:
    vPortFree(ctx);
    return NULL;
}

void dxl_hal_set_baud_rate(dxl_hal_context_t *ctx, u32 baud_rate) {
    ctx->baud_rate = baud_rate;

    float div = (float)clock_get_hz(clk_sys) / (8.0f * (float)baud_rate);

    pio_sm_set_clkdiv(ctx->pio, ctx->sm_tx, div);
    pio_sm_set_clkdiv(ctx->pio, ctx->sm_rx, div);

    info("dynamixel HAL: baud rate changed to %lu", baud_rate);
}

u32 dxl_hal_get_baud_rate(dxl_hal_context_t *ctx) { return ctx->baud_rate; }

dxl_packet_t *dxl_hal_work_pkt(dxl_hal_context_t *ctx) { return &ctx->work_pkt; }

dxl_packet_t *dxl_hal_multi_pkt_buf(dxl_hal_context_t *ctx) { return ctx->multi_rx_pkts; }

u8 dxl_hal_last_servo_error(dxl_hal_context_t *ctx) { return ctx->last_servo_error; }

void dxl_hal_flush_rx(dxl_hal_context_t *ctx) {
    // Abort any in-progress DMA
    dma_channel_abort(ctx->dma_chan);

    // Drain the RX FIFO (bounded to prevent infinite loop from bus noise)
    for (u32 i = 0; i < DXL_MAX_PACKET_SIZE * 2 && !pio_sm_is_rx_fifo_empty(ctx->pio, ctx->sm_rx); i++) {
        (void)pio_sm_get(ctx->pio, ctx->sm_rx);
    }
}

dxl_result_t dxl_hal_txrx(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt, dxl_packet_t *rx_pkt, u32 timeout_ms) {
    ctx->last_servo_error = 0;

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

    // Step 3-4: Write packet bytes via PIO and wait for completion (with timeout)
    absolute_time_t tx_deadline = make_tx_deadline(ctx->baud_rate);
    for (size_t i = 0; i < tx_len; i++) {
        if (pio_put_with_timeout(ctx->pio, ctx->sm_tx, (u32)tx_buf[i], tx_deadline) != DXL_OK) {
            pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
            pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);
            return DXL_TIMEOUT;
        }
    }

    if (wait_tx_complete(ctx->pio, ctx->sm_tx, ctx->baud_rate, tx_deadline) != DXL_OK) {
        pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
        pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);
        return DXL_TIMEOUT;
    }

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
    dxl_result_t parse_res = dxl_parse_packet(ctx->rx_buffer, bytes_received, rx_pkt);
    if (parse_res == DXL_SERVO_ERROR) {
        ctx->last_servo_error = rx_pkt->error;
    }
    return parse_res;
}

dxl_result_t dxl_hal_txrx_multi(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt, u16 data_per_response,
                                u8 expected_count, dxl_packet_t *rx_pkts, u8 *received_count, u32 timeout_ms) {
    ctx->last_servo_error = 0;
    *received_count = 0;

    // Build wire packet
    u8 tx_buf[DXL_MAX_PACKET_SIZE];
    size_t tx_len = 0;

    dxl_result_t result = dxl_build_packet(tx_pkt, tx_buf, sizeof(tx_buf), &tx_len);
    if (result != DXL_OK) {
        return result;
    }

    // Each response: header(4) + id(1) + length(2) + instruction(1) + error(1) + data + crc(2) = 11 + data
    u32 bytes_per_response = (u32)data_per_response + 11;
    u32 rx_buf_size = bytes_per_response * expected_count;
    if (rx_buf_size > DXL_MULTI_RX_BUF_SIZE) {
        error("dxl_hal_txrx_multi: RX buffer too small (%lu > %u)", rx_buf_size, DXL_MULTI_RX_BUF_SIZE);
        return DXL_BUFFER_OVERFLOW;
    }
    u8 *rx_buf = ctx->multi_rx_buf;
    memset(rx_buf, 0, rx_buf_size);

    // Step 1: Flush any stale RX data
    dxl_hal_flush_rx(ctx);

    // Step 2: Set TX SM pindir to output and enable TX
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 1u << ctx->data_pin, 1u << ctx->data_pin);
    pio_sm_restart(ctx->pio, ctx->sm_tx);
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, true);

    // Step 3-4: Write packet bytes via PIO and wait for completion (with timeout)
    absolute_time_t tx_deadline = make_tx_deadline(ctx->baud_rate);
    for (size_t i = 0; i < tx_len; i++) {
        if (pio_put_with_timeout(ctx->pio, ctx->sm_tx, (u32)tx_buf[i], tx_deadline) != DXL_OK) {
            pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
            pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);
            return DXL_TIMEOUT;
        }
    }

    if (wait_tx_complete(ctx->pio, ctx->sm_tx, ctx->baud_rate, tx_deadline) != DXL_OK) {
        pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
        pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);
        return DXL_TIMEOUT;
    }

    // Step 5: Disable TX SM and release pin direction so RX can use it
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);

    // Step 6: Enable RX SM
    pio_sm_restart(ctx->pio, ctx->sm_rx);
    pio_sm_set_enabled(ctx->pio, ctx->sm_rx, true);

    // Step 7: Configure DMA for RX
    dma_channel_config dma_cfg = dma_channel_get_default_config(ctx->dma_chan);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_dreq(&dma_cfg, pio_get_dreq(ctx->pio, ctx->sm_rx, false));

    volatile void *rx_fifo_addr = (volatile void *)((uintptr_t)&ctx->pio->rxf[ctx->sm_rx] + 3);

    dma_channel_configure(ctx->dma_chan, &dma_cfg,
                          rx_buf,       // write address
                          rx_fifo_addr, // read address (upper byte of RX FIFO)
                          rx_buf_size,  // max transfer count
                          true          // start immediately
    );

    // Step 8: Wait for RX with alarm-based idle detection
    // Instead of busy-waiting, a hardware alarm fires periodically and checks
    // if new bytes have arrived via DMA. When the bus goes idle (no new bytes
    // for idle_limit consecutive checks), the alarm signals a semaphore.
    // This lets the calling task yield to the scheduler during the entire
    // receive window (~6-7ms at 1Mbaud with 8 servos).

    // Compute alarm timing from baud rate
    u32 byte_time_us = 10000000 / ctx->baud_rate; // 10 bits per byte (8N1)
    u32 alarm_period_us = 3 * byte_time_us;
    if (alarm_period_us < 100)
        alarm_period_us = 100;
    if (alarm_period_us > 500)
        alarm_period_us = 500;

    u32 idle_timeout_us = 15 * byte_time_us + 1000;
    u32 idle_limit = idle_timeout_us / alarm_period_us + 1;

    // Initialize alarm state
    ctx->alarm_state.last_seen_bytes = 0;
    ctx->alarm_state.idle_count = 0;
    ctx->alarm_state.idle_limit = idle_limit;
    ctx->alarm_state.rx_buf_size = rx_buf_size;
    ctx->alarm_state.dma_chan = ctx->dma_chan;
    ctx->alarm_state.alarm_period_us = alarm_period_us;

    // Drain any stale semaphore signal
    xSemaphoreTake(ctx->rx_complete_sem, 0);

    // Start the repeating alarm on our dedicated FreeRTOS-safe pool
    alarm_id_t rx_alarm = alarm_pool_add_alarm_in_us(ctx->alarm_pool, alarm_period_us, dxl_rxmulti_alarm_callback,
                                                     &ctx->alarm_state, true);

    size_t bytes_received;

    if (rx_alarm >= 0) {
        // Block until the alarm detects bus idle, or timeout
        xSemaphoreTake(ctx->rx_complete_sem, pdMS_TO_TICKS(timeout_ms));
        alarm_pool_cancel_alarm(ctx->alarm_pool, rx_alarm);
    } else {
        // Alarm pool full (very unlikely) — fall back to yielding poll loop
        warning("dxl_hal_txrx_multi: alarm pool full, falling back to poll loop");
        absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
        u32 last_seen = 0;
        u32 idle_checks = 0;

        while (!time_reached(deadline)) {
            u32 remaining = dma_channel_hw_addr(ctx->dma_chan)->transfer_count;
            u32 current = rx_buf_size - remaining;

            if (current != last_seen) {
                last_seen = current;
                idle_checks = 0;
            } else if (last_seen > 0) {
                idle_checks++;
                if (idle_checks >= idle_limit) {
                    break;
                }
            }

            vTaskDelay(1);
        }
    }

    // Step 9: Abort DMA and disable RX SM
    dma_channel_abort(ctx->dma_chan);
    pio_sm_set_enabled(ctx->pio, ctx->sm_rx, false);

    // Recheck final count
    u32 final_remaining = dma_channel_hw_addr(ctx->dma_chan)->transfer_count;
    bytes_received = rx_buf_size - final_remaining;

    // Step 10: Parse individual packets from the buffer
    size_t offset = 0;
    u8 count = 0;

    while (offset < bytes_received && count < expected_count) {
        // Scan for packet header: FF FF FD 00
        while (offset + 10 <= bytes_received) {
            if (rx_buf[offset] == DXL_HEADER_0 && rx_buf[offset + 1] == DXL_HEADER_1 &&
                rx_buf[offset + 2] == DXL_HEADER_2 && rx_buf[offset + 3] == DXL_RESERVED) {
                break;
            }
            offset++;
        }

        if (offset + 10 > bytes_received) {
            break;
        }

        // Read wire_length to determine packet size
        if (offset + 7 > bytes_received) {
            break;
        }
        u16 wire_length = (u16)rx_buf[offset + 5] | ((u16)rx_buf[offset + 6] << 8);
        size_t pkt_total = 7 + (size_t)wire_length;

        if (offset + pkt_total > bytes_received) {
            break;
        }

        dxl_result_t parse_res = dxl_parse_packet(&rx_buf[offset], pkt_total, &rx_pkts[count]);
        if (parse_res == DXL_OK || parse_res == DXL_SERVO_ERROR) {
            count++;
        }

        offset += pkt_total;
    }

    *received_count = count;

    return (count > 0) ? DXL_OK : DXL_TIMEOUT;
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

    // Write packet bytes (with timeout)
    absolute_time_t tx_deadline = make_tx_deadline(ctx->baud_rate);
    for (size_t i = 0; i < tx_len; i++) {
        if (pio_put_with_timeout(ctx->pio, ctx->sm_tx, (u32)tx_buf[i], tx_deadline) != DXL_OK) {
            pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
            pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);
            return DXL_TIMEOUT;
        }
    }

    // Wait for TX to complete (with timeout)
    if (wait_tx_complete(ctx->pio, ctx->sm_tx, ctx->baud_rate, tx_deadline) != DXL_OK) {
        pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
        pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);
        return DXL_TIMEOUT;
    }

    // Disable TX SM and release pin direction back to input
    pio_sm_set_enabled(ctx->pio, ctx->sm_tx, false);
    pio_sm_set_pindirs_with_mask(ctx->pio, ctx->sm_tx, 0, 1u << ctx->data_pin);

    return DXL_OK;
}

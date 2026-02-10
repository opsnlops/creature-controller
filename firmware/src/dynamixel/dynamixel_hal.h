
#pragma once

/**
 * @file dynamixel_hal.h
 * @brief Hardware abstraction for Dynamixel communication
 *
 * PIO-based half-duplex UART using a single GPIO pin. Uses two PIO state
 * machines (TX and RX) on the same pin, with DMA for receive.
 */

#include <stdbool.h>

#include "hardware/pio.h"

#include "dynamixel_protocol.h"
#include "types.h"

/**
 * @brief Configuration for the Dynamixel HAL
 */
typedef struct {
    u32 data_pin;  // GPIO pin for the half-duplex data line
    u32 baud_rate; // Baud rate in bps
    PIO pio;       // PIO instance to use (pio0 or pio1)
} dxl_hal_config_t;

/**
 * @brief Opaque HAL context (allocated by init)
 */
typedef struct dxl_hal_context dxl_hal_context_t;

/**
 * @brief Initialize the Dynamixel HAL
 *
 * Loads TX and RX PIO programs, claims state machines and a DMA channel,
 * and configures both SMs on the specified GPIO pin.
 *
 * TX SM starts disabled (enabled only during transmit).
 * RX SM starts disabled (enabled only during receive).
 *
 * @param config Hardware configuration
 * @return Pointer to the HAL context, or NULL on failure
 */
dxl_hal_context_t *dxl_hal_init(const dxl_hal_config_t *config);

/**
 * @brief Change the baud rate
 *
 * Reconfigures both TX and RX state machine clock dividers.
 *
 * @param ctx HAL context
 * @param baud_rate New baud rate in bps
 */
void dxl_hal_set_baud_rate(dxl_hal_context_t *ctx, u32 baud_rate);

/**
 * @brief Send a packet and wait for a response
 *
 * Full TX/RX cycle:
 * 1. Flush RX FIFO
 * 2. Enable TX SM, send packet bytes, wait for completion
 * 3. Disable TX SM, enable RX SM
 * 4. DMA receive into buffer, monitor for expected length
 * 5. Disable RX SM, parse response
 *
 * @param ctx HAL context
 * @param tx_pkt Packet to transmit
 * @param rx_pkt Parsed response (output)
 * @param timeout_ms Maximum time to wait for response
 * @return DXL_OK on success, error code on failure
 */
dxl_result_t dxl_hal_txrx(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt, dxl_packet_t *rx_pkt, u32 timeout_ms);

/**
 * @brief Transmit a packet without waiting for a response
 *
 * Used for broadcast commands where no response is expected.
 *
 * @param ctx HAL context
 * @param tx_pkt Packet to transmit
 * @return DXL_OK on success, error code on failure
 */
dxl_result_t dxl_hal_tx(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt);

/**
 * @brief Flush the RX FIFO and abort any in-progress DMA
 *
 * @param ctx HAL context
 */
void dxl_hal_flush_rx(dxl_hal_context_t *ctx);

/**
 * @brief Get the current baud rate
 *
 * @param ctx HAL context
 * @return Current baud rate in bps
 */
u32 dxl_hal_get_baud_rate(dxl_hal_context_t *ctx);

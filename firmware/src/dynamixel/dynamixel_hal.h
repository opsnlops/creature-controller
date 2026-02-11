
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
 * @brief Maximum number of responses from a single multi-response transaction
 */
#define DXL_MAX_MULTI_RESPONSES 16

/**
 * @brief Pre-allocated RX buffer size for multi-response DMA
 */
#define DXL_MULTI_RX_BUF_SIZE (DXL_MAX_PACKET_SIZE * DXL_MAX_MULTI_RESPONSES)

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
 * @brief Send a packet and receive multiple response packets
 *
 * Used for Sync Read where a single broadcast request produces
 * individual response packets from each addressed servo.
 *
 * @param ctx HAL context
 * @param tx_pkt Packet to transmit
 * @param data_per_response Expected data bytes per response (excluding protocol overhead)
 * @param expected_count Number of responses expected
 * @param rx_pkts Array of parsed response packets (output, must hold expected_count entries)
 * @param received_count Number of packets successfully parsed (output)
 * @param timeout_ms Maximum time to wait for all responses
 * @return DXL_OK if at least one packet received, DXL_TIMEOUT if zero
 */
dxl_result_t dxl_hal_txrx_multi(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt, u16 data_per_response,
                                u8 expected_count, dxl_packet_t *rx_pkts, u8 *received_count, u32 timeout_ms);

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

/**
 * @brief Get the pre-allocated workspace packet for building TX packets
 *
 * Avoids per-call heap allocation in hot paths. The returned pointer is
 * valid for the lifetime of the HAL context. Not reentrant — only one
 * caller should use this at a time.
 *
 * @param ctx HAL context
 * @return Pointer to a reusable dxl_packet_t
 */
dxl_packet_t *dxl_hal_work_pkt(dxl_hal_context_t *ctx);

/**
 * @brief Get the pre-allocated packet array for multi-response parsing
 *
 * Returns an array of DXL_MAX_MULTI_RESPONSES packets. Avoids per-call
 * heap allocation in hot paths. Not reentrant.
 *
 * @param ctx HAL context
 * @return Pointer to a reusable array of dxl_packet_t
 */
dxl_packet_t *dxl_hal_multi_pkt_buf(dxl_hal_context_t *ctx);

/**
 * @brief Get the protocol error byte from the most recent servo response
 *
 * After any txrx operation returns DXL_SERVO_ERROR, this returns the
 * specific error code from the servo's status packet (e.g. DXL_ERR_DATA_RANGE,
 * DXL_ERR_ACCESS). Cleared at the start of each transaction.
 *
 * @param ctx HAL context
 * @return Error byte from the last status packet, or 0 if no error
 */
u8 dxl_hal_last_servo_error(dxl_hal_context_t *ctx);

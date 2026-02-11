
#pragma once

/**
 * @file dynamixel_protocol.h
 * @brief Dynamixel Protocol 2.0 packet layer
 *
 * Handles serialization and deserialization of Protocol 2.0 packets,
 * including CRC16 calculation and byte stuffing.
 */

#include <stdbool.h>
#include <stddef.h>

#include "dynamixel_registers.h"
#include "types.h"

// Maximum parameter bytes in a single packet
#define DXL_MAX_PARAMS (DXL_MAX_PACKET_SIZE - 10)

/**
 * @brief Result codes for Dynamixel operations
 */
typedef enum {
    DXL_OK = 0,
    DXL_TIMEOUT,
    DXL_INVALID_PACKET,
    DXL_CRC_MISMATCH,
    DXL_SERVO_ERROR,
    DXL_TX_FAIL,
    DXL_BUFFER_OVERFLOW,
} dxl_result_t;

/**
 * @brief Represents a Dynamixel Protocol 2.0 packet
 */
typedef struct {
    u8 id;
    u8 instruction;
    u8 error;
    u8 params[DXL_MAX_PARAMS];
    u16 param_count;
} dxl_packet_t;

/**
 * @brief Calculate the Dynamixel CRC16
 *
 * Uses the standard Dynamixel 256-entry lookup table.
 *
 * @param data Pointer to data buffer
 * @param length Number of bytes to process
 * @return CRC16 value
 */
u16 dxl_crc16(const u8 *data, size_t length);

/**
 * @brief Build a wire-format packet from a packet struct
 *
 * Serializes the packet into wire bytes, including the header, length,
 * instruction, parameters (with byte stuffing), and CRC16.
 *
 * @param pkt Source packet struct
 * @param out_buf Output buffer for wire bytes
 * @param out_buf_size Size of the output buffer
 * @param out_length Set to the number of bytes written
 * @return DXL_OK on success, DXL_BUFFER_OVERFLOW if buffer is too small
 */
dxl_result_t dxl_build_packet(const dxl_packet_t *pkt, u8 *out_buf, size_t out_buf_size, size_t *out_length);

/**
 * @brief Parse wire bytes into a packet struct
 *
 * Deserializes wire bytes back to a packet struct, validating the header,
 * checking the CRC, and removing byte stuffing from parameters.
 *
 * @param data Wire-format bytes
 * @param length Number of bytes available
 * @param pkt Output packet struct
 * @return DXL_OK on success, error code otherwise
 */
dxl_result_t dxl_parse_packet(const u8 *data, size_t length, dxl_packet_t *pkt);

/**
 * @brief Convert a result code to a human-readable string
 */
const char *dxl_result_to_string(dxl_result_t result);

/**
 * @brief Convert a Dynamixel protocol error byte to a human-readable string
 */
const char *dxl_error_to_string(u8 error);

/**
 * @brief Decode hardware error status register bits into a human-readable string
 *
 * The Hardware Error Status register (addr 70) is a bitmask. This function
 * writes a comma-separated list of active error flags into the provided buffer.
 * If no bits are set, writes "none".
 *
 * @param hw_error Hardware error register value
 * @param buf Output buffer
 * @param buf_size Size of the output buffer
 * @return Number of characters written (excluding null terminator)
 */
size_t dxl_hw_error_to_string(u8 hw_error, char *buf, size_t buf_size);

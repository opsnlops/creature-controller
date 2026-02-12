
#include <string.h>

#include <FreeRTOS.h>

#include "logging/logging.h"

#include "dynamixel_registers.h"
#include "dynamixel_servo.h"

static const u32 baud_rate_table[] = {
    9600,    // index 0
    57600,   // index 1
    115200,  // index 2
    1000000, // index 3
    2000000, // index 4
    3000000, // index 5
    4000000, // index 6
    4500000, // index 7
};

#define BAUD_RATE_TABLE_SIZE (sizeof(baud_rate_table) / sizeof(baud_rate_table[0]))

u32 dxl_baud_index_to_rate(u8 index) {
    if (index >= BAUD_RATE_TABLE_SIZE) {
        return 0;
    }
    return baud_rate_table[index];
}

dxl_result_t dxl_ping(dxl_hal_context_t *ctx, u8 id, dxl_ping_result_t *result) {
    dxl_packet_t tx_pkt;
    memset(&tx_pkt, 0, sizeof(tx_pkt));
    tx_pkt.id = id;
    tx_pkt.instruction = DXL_INST_PING;
    tx_pkt.param_count = 0;

    dxl_packet_t rx_pkt;
    memset(&rx_pkt, 0, sizeof(rx_pkt));

    dxl_result_t res = dxl_hal_txrx(ctx, &tx_pkt, &rx_pkt, DXL_DEFAULT_TIMEOUT_MS);
    if (res != DXL_OK) {
        return res;
    }

    // Ping response has 3 parameter bytes: model_number(2) + firmware_version(1)
    if (rx_pkt.param_count >= 3) {
        result->model_number = (u16)rx_pkt.params[0] | ((u16)rx_pkt.params[1] << 8);
        result->firmware_version = rx_pkt.params[2];
    }

    return DXL_OK;
}

void dxl_scan(dxl_hal_context_t *ctx, u8 start_id, u8 end_id, dxl_scan_callback_t callback) {
    for (u16 id = start_id; id <= end_id; id++) {
        // Inline ping with short scan timeout (DXL_SCAN_TIMEOUT_MS vs 100ms default)
        // so empty IDs don't stall the scan
        dxl_packet_t tx_pkt;
        memset(&tx_pkt, 0, sizeof(tx_pkt));
        tx_pkt.id = (u8)id;
        tx_pkt.instruction = DXL_INST_PING;

        dxl_packet_t rx_pkt;
        memset(&rx_pkt, 0, sizeof(rx_pkt));

        dxl_result_t res = dxl_hal_txrx(ctx, &tx_pkt, &rx_pkt, DXL_SCAN_TIMEOUT_MS);
        if (res == DXL_OK && rx_pkt.param_count >= 3) {
            u16 model = (u16)rx_pkt.params[0] | ((u16)rx_pkt.params[1] << 8);
            callback((u8)id, model, rx_pkt.params[2]);
        }
    }
}

dxl_result_t dxl_read_register(dxl_hal_context_t *ctx, u8 id, u16 address, u16 length, u32 *value) {
    dxl_packet_t tx_pkt;
    memset(&tx_pkt, 0, sizeof(tx_pkt));
    tx_pkt.id = id;
    tx_pkt.instruction = DXL_INST_READ;
    tx_pkt.param_count = 4;
    tx_pkt.params[0] = (u8)(address & 0xFF);
    tx_pkt.params[1] = (u8)((address >> 8) & 0xFF);
    tx_pkt.params[2] = (u8)(length & 0xFF);
    tx_pkt.params[3] = (u8)((length >> 8) & 0xFF);

    dxl_packet_t rx_pkt;
    memset(&rx_pkt, 0, sizeof(rx_pkt));

    dxl_result_t res = dxl_hal_txrx(ctx, &tx_pkt, &rx_pkt, DXL_DEFAULT_TIMEOUT_MS);
    if (res != DXL_OK) {
        return res;
    }

    // Reconstruct the value from the response parameters (little-endian)
    *value = 0;
    for (u16 i = 0; i < length && i < rx_pkt.param_count; i++) {
        *value |= ((u32)rx_pkt.params[i]) << (8 * i);
    }

    return DXL_OK;
}

dxl_result_t dxl_write_register(dxl_hal_context_t *ctx, u8 id, u16 address, u16 length, u32 value) {
    dxl_packet_t tx_pkt;
    memset(&tx_pkt, 0, sizeof(tx_pkt));
    tx_pkt.id = id;
    tx_pkt.instruction = DXL_INST_WRITE;
    tx_pkt.param_count = 2 + length;
    tx_pkt.params[0] = (u8)(address & 0xFF);
    tx_pkt.params[1] = (u8)((address >> 8) & 0xFF);

    for (u16 i = 0; i < length; i++) {
        tx_pkt.params[2 + i] = (u8)((value >> (8 * i)) & 0xFF);
    }

    dxl_packet_t rx_pkt;
    memset(&rx_pkt, 0, sizeof(rx_pkt));

    return dxl_hal_txrx(ctx, &tx_pkt, &rx_pkt, DXL_DEFAULT_TIMEOUT_MS);
}

/**
 * @brief Check that torque is off before writing EEPROM registers.
 *
 * Skipped for broadcast ID (can't read back from broadcast).
 */
static dxl_result_t check_torque_off(dxl_hal_context_t *ctx, u8 id) {
    if (id == DXL_BROADCAST_ID) {
        return DXL_OK;
    }
    u32 torque = 0;
    dxl_result_t res = dxl_read_register(ctx, id, DXL_REG_TORQUE_ENABLE, 1, &torque);
    if (res != DXL_OK) {
        return res;
    }
    return torque ? DXL_TORQUE_ENABLED : DXL_OK;
}

dxl_result_t dxl_set_id(dxl_hal_context_t *ctx, u8 current_id, u8 new_id) {
    dxl_result_t res = check_torque_off(ctx, current_id);
    if (res != DXL_OK) {
        return res;
    }
    return dxl_write_register(ctx, current_id, DXL_REG_ID, 1, new_id);
}

dxl_result_t dxl_set_baud_rate(dxl_hal_context_t *ctx, u8 id, u8 baud_index) {
    dxl_result_t res = check_torque_off(ctx, id);
    if (res != DXL_OK) {
        return res;
    }
    return dxl_write_register(ctx, id, DXL_REG_BAUD_RATE, 1, baud_index);
}

dxl_result_t dxl_factory_reset(dxl_hal_context_t *ctx, u8 id, u8 option) {
    dxl_packet_t tx_pkt;
    memset(&tx_pkt, 0, sizeof(tx_pkt));
    tx_pkt.id = id;
    tx_pkt.instruction = DXL_INST_FACTORY_RESET;
    tx_pkt.param_count = 1;
    tx_pkt.params[0] = option;

    dxl_packet_t rx_pkt;
    memset(&rx_pkt, 0, sizeof(rx_pkt));

    return dxl_hal_txrx(ctx, &tx_pkt, &rx_pkt, DXL_DEFAULT_TIMEOUT_MS);
}

dxl_result_t dxl_reboot(dxl_hal_context_t *ctx, u8 id) {
    dxl_packet_t tx_pkt;
    memset(&tx_pkt, 0, sizeof(tx_pkt));
    tx_pkt.id = id;
    tx_pkt.instruction = DXL_INST_REBOOT;
    tx_pkt.param_count = 0;

    dxl_packet_t rx_pkt;
    memset(&rx_pkt, 0, sizeof(rx_pkt));

    return dxl_hal_txrx(ctx, &tx_pkt, &rx_pkt, DXL_DEFAULT_TIMEOUT_MS);
}

dxl_result_t dxl_set_torque(dxl_hal_context_t *ctx, u8 id, bool enable) {
    return dxl_write_register(ctx, id, DXL_REG_TORQUE_ENABLE, 1, enable ? 1 : 0);
}

dxl_result_t dxl_set_position(dxl_hal_context_t *ctx, u8 id, u32 position) {
    return dxl_write_register(ctx, id, DXL_REG_GOAL_POSITION, 4, position);
}

dxl_result_t dxl_set_led(dxl_hal_context_t *ctx, u8 id, bool on) {
    return dxl_write_register(ctx, id, DXL_REG_LED, 1, on ? 1 : 0);
}

dxl_result_t dxl_set_profile_velocity(dxl_hal_context_t *ctx, u8 id, u32 velocity) {
    return dxl_write_register(ctx, id, DXL_REG_PROFILE_VELOCITY, 4, velocity);
}

dxl_result_t dxl_sync_write_position(dxl_hal_context_t *ctx, const dxl_sync_position_t *entries, u8 count) {
    if (count == 0 || count > DXL_MAX_SYNC_SERVOS) {
        return DXL_INVALID_PACKET;
    }

    // Use pre-allocated workspace from HAL context
    dxl_packet_t *tx_pkt = dxl_hal_work_pkt(ctx);

    // Build Sync Write packet: id=0xFE (broadcast), instruction=0x83
    // Params: [start_addr_L, start_addr_H, data_len_L, data_len_H,
    //          ID1, pos1_0, pos1_1, pos1_2, pos1_3,
    //          ID2, pos2_0, pos2_1, pos2_2, pos2_3, ...]
    memset(tx_pkt, 0, sizeof(dxl_packet_t));
    tx_pkt->id = DXL_BROADCAST_ID;
    tx_pkt->instruction = DXL_INST_SYNC_WRITE;

    u16 data_len = 4; // Goal Position is 4 bytes
    tx_pkt->params[0] = (u8)(DXL_REG_GOAL_POSITION & 0xFF);
    tx_pkt->params[1] = (u8)((DXL_REG_GOAL_POSITION >> 8) & 0xFF);
    tx_pkt->params[2] = (u8)(data_len & 0xFF);
    tx_pkt->params[3] = (u8)((data_len >> 8) & 0xFF);

    u16 offset = 4;
    for (u8 i = 0; i < count; i++) {
        tx_pkt->params[offset++] = entries[i].id;
        tx_pkt->params[offset++] = (u8)(entries[i].position & 0xFF);
        tx_pkt->params[offset++] = (u8)((entries[i].position >> 8) & 0xFF);
        tx_pkt->params[offset++] = (u8)((entries[i].position >> 16) & 0xFF);
        tx_pkt->params[offset++] = (u8)((entries[i].position >> 24) & 0xFF);
    }
    tx_pkt->param_count = offset;

    // Sync Write is broadcast — no response expected
    return dxl_hal_tx(ctx, tx_pkt);
}

// Sync Read register block: addresses 126-146 (21 bytes)
// Offset 0:  Present Load (2 bytes)
// Offset 2:  Present Velocity (4 bytes)
// Offset 6:  Present Position (4 bytes)
// Offset 10: Velocity Trajectory (4 bytes, not used)
// Offset 14: Position Trajectory (4 bytes, not used)
// Offset 18: Present Input Voltage (2 bytes)
// Offset 20: Present Temperature (1 byte)
#define SYNC_READ_START_ADDR 126
#define SYNC_READ_DATA_LENGTH 21

dxl_result_t dxl_sync_read_status(dxl_hal_context_t *ctx, const u8 *ids, u8 id_count, dxl_sync_status_result_t *results,
                                  u8 *result_count) {
    *result_count = 0;

    if (id_count == 0 || id_count > DXL_MAX_SYNC_SERVOS) {
        return DXL_INVALID_PACKET;
    }

    // Initialize all results
    for (u8 i = 0; i < id_count; i++) {
        results[i].id = ids[i];
        memset(&results[i].status, 0, sizeof(dxl_servo_status_t));
        results[i].valid = false;
        results[i].servo_error = 0;
    }

    // Use pre-allocated workspace from HAL context (avoids per-frame heap allocation)
    dxl_packet_t *tx_pkt = dxl_hal_work_pkt(ctx);
    dxl_packet_t *rx_pkts = dxl_hal_multi_pkt_buf(ctx);

    // Build Sync Read packet: id=0xFE, instruction=0x82
    // Params: [start_addr_L, start_addr_H, data_len_L, data_len_H, id1, id2, ...]
    memset(tx_pkt, 0, sizeof(dxl_packet_t));
    tx_pkt->id = DXL_BROADCAST_ID;
    tx_pkt->instruction = DXL_INST_SYNC_READ;
    tx_pkt->param_count = 4 + id_count;
    tx_pkt->params[0] = (u8)(SYNC_READ_START_ADDR & 0xFF);
    tx_pkt->params[1] = (u8)((SYNC_READ_START_ADDR >> 8) & 0xFF);
    tx_pkt->params[2] = (u8)(SYNC_READ_DATA_LENGTH & 0xFF);
    tx_pkt->params[3] = (u8)((SYNC_READ_DATA_LENGTH >> 8) & 0xFF);
    for (u8 i = 0; i < id_count; i++) {
        tx_pkt->params[4 + i] = ids[i];
    }

    memset(rx_pkts, 0, sizeof(dxl_packet_t) * id_count);
    u8 received = 0;

    // Timeout scaled to baud rate: expected wire time * 2 + margin
    u32 baud = dxl_hal_get_baud_rate(ctx);
    u32 byte_time_us = 10000000 / baud;
    u32 expected_us = (u32)id_count * ((SYNC_READ_DATA_LENGTH + 11) * byte_time_us + 500) + 15 * byte_time_us + 1000;
    u32 timeout = expected_us / 500 + 5; // ~2x expected + 5ms margin

    dxl_result_t res = dxl_hal_txrx_multi(ctx, tx_pkt, SYNC_READ_DATA_LENGTH, id_count, rx_pkts, &received, timeout);

    if (res != DXL_OK) {
        return res;
    }

    // Match response packets to our result array by servo ID
    for (u8 r = 0; r < received; r++) {
        u8 resp_id = rx_pkts[r].id;

        for (u8 i = 0; i < id_count; i++) {
            if (results[i].id == resp_id && !results[i].valid) {
                results[i].servo_error = rx_pkts[r].error;

                if (rx_pkts[r].param_count >= SYNC_READ_DATA_LENGTH) {
                    const u8 *d = rx_pkts[r].params;

                    // Offset 0: Present Load (2 bytes, little-endian, signed)
                    results[i].status.present_load = (i16)((u16)d[0] | ((u16)d[1] << 8));

                    // Offset 6: Present Position (4 bytes, little-endian, signed)
                    results[i].status.present_position =
                        (i32)((u32)d[6] | ((u32)d[7] << 8) | ((u32)d[8] << 16) | ((u32)d[9] << 24));

                    // Offset 18: Present Input Voltage (2 bytes, little-endian)
                    results[i].status.present_voltage = (u16)d[18] | ((u16)d[19] << 8);

                    // Offset 20: Present Temperature (1 byte)
                    results[i].status.present_temperature = d[20];

                    // moving and hardware_error are outside this block, left as 0
                    results[i].valid = true;
                }
                break;
            }
        }
    }

    // Count valid results
    u8 valid_count = 0;
    for (u8 i = 0; i < id_count; i++) {
        if (results[i].valid) {
            valid_count++;
        }
    }
    *result_count = valid_count;

    return DXL_OK;
}

dxl_result_t dxl_read_status(dxl_hal_context_t *ctx, u8 id, dxl_servo_status_t *status) {
    memset(status, 0, sizeof(dxl_servo_status_t));

    u32 val = 0;
    dxl_result_t res;

    res = dxl_read_register(ctx, id, DXL_REG_PRESENT_POSITION, 4, &val);
    if (res != DXL_OK)
        return res;
    status->present_position = (i32)val;

    res = dxl_read_register(ctx, id, DXL_REG_PRESENT_TEMPERATURE, 1, &val);
    if (res != DXL_OK)
        return res;
    status->present_temperature = (u8)val;

    res = dxl_read_register(ctx, id, DXL_REG_PRESENT_INPUT_VOLT, 2, &val);
    if (res != DXL_OK)
        return res;
    status->present_voltage = (u16)val;

    res = dxl_read_register(ctx, id, DXL_REG_PRESENT_LOAD, 2, &val);
    if (res != DXL_OK)
        return res;
    status->present_load = (i16)val;

    res = dxl_read_register(ctx, id, DXL_REG_MOVING, 1, &val);
    if (res != DXL_OK)
        return res;
    status->moving = (u8)val;

    res = dxl_read_register(ctx, id, DXL_REG_HARDWARE_ERROR, 1, &val);
    if (res != DXL_OK)
        return res;
    status->hardware_error = (u8)val;

    return DXL_OK;
}

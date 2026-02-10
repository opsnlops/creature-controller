
#include <string.h>

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
        dxl_ping_result_t result;
        memset(&result, 0, sizeof(result));

        dxl_result_t res = dxl_ping(ctx, (u8)id, &result);
        if (res == DXL_OK) {
            callback((u8)id, result.model_number, result.firmware_version);
        }
        // Timeouts and errors are expected for empty IDs, just skip
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

dxl_result_t dxl_set_id(dxl_hal_context_t *ctx, u8 current_id, u8 new_id) {
    return dxl_write_register(ctx, current_id, DXL_REG_ID, 1, new_id);
}

dxl_result_t dxl_set_baud_rate(dxl_hal_context_t *ctx, u8 id, u8 baud_index) {
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

dxl_result_t dxl_read_status(dxl_hal_context_t *ctx, u8 id, dxl_servo_status_t *status) {
    memset(status, 0, sizeof(dxl_servo_status_t));

    u32 val = 0;
    dxl_result_t res;

    res = dxl_read_register(ctx, id, DXL_REG_PRESENT_POSITION, 4, &val);
    if (res != DXL_OK)
        return res;
    status->present_position = val;

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
    status->present_load = (u16)val;

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

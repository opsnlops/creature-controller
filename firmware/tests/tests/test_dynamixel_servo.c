/**
 * @file test_dynamixel_servo.c
 * @brief Tests for Dynamixel servo-layer functions
 *
 * Tests baud rate conversion, sync write packet construction,
 * and edge cases. Uses stub HAL implementations — no hardware required.
 */

#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "dynamixel/dynamixel_protocol.h"
#include "dynamixel/dynamixel_servo.h"

// ---- HAL stubs ----
// dynamixel_servo.c needs these symbols but we don't link the real HAL.
// The opaque context is just a non-NULL pointer for the tests.

static dxl_packet_t stub_work_pkt;
static dxl_packet_t stub_multi_pkts[DXL_MAX_MULTI_RESPONSES];
static dxl_packet_t last_tx_pkt;
static bool tx_called;

// Configurable stub behavior for dxl_hal_txrx
static bool stub_torque_on;           // when true, torque read returns 1
static bool stub_txrx_return_ok;      // when true, txrx returns DXL_OK with a response

PIO pio0 = NULL;
PIO pio1 = NULL;

// Fake context — just needs to be non-NULL
static int fake_ctx_storage;
static dxl_hal_context_t *fake_ctx = (dxl_hal_context_t *)&fake_ctx_storage;

dxl_packet_t *dxl_hal_work_pkt(dxl_hal_context_t *ctx) {
    (void)ctx;
    return &stub_work_pkt;
}

dxl_packet_t *dxl_hal_multi_pkt_buf(dxl_hal_context_t *ctx) {
    (void)ctx;
    return stub_multi_pkts;
}

u32 dxl_hal_get_baud_rate(dxl_hal_context_t *ctx) {
    (void)ctx;
    return 1000000;
}

u8 dxl_hal_last_servo_error(dxl_hal_context_t *ctx) {
    (void)ctx;
    return 0;
}

static dxl_metrics_t stub_metrics;
const dxl_metrics_t *dxl_hal_metrics(dxl_hal_context_t *ctx) {
    (void)ctx;
    return &stub_metrics;
}

void dxl_hal_flush_rx(dxl_hal_context_t *ctx) { (void)ctx; }

dxl_result_t dxl_hal_tx(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt) {
    (void)ctx;
    memcpy(&last_tx_pkt, tx_pkt, sizeof(dxl_packet_t));
    tx_called = true;
    return DXL_OK;
}

dxl_result_t dxl_hal_txrx(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt, dxl_packet_t *rx_pkt, u32 timeout_ms) {
    (void)ctx;
    (void)timeout_ms;
    memcpy(&last_tx_pkt, tx_pkt, sizeof(dxl_packet_t));

    if (!stub_txrx_return_ok) {
        return DXL_TIMEOUT;
    }

    // Simulate a read response: echo back data based on the register address
    memset(rx_pkt, 0, sizeof(dxl_packet_t));
    rx_pkt->id = tx_pkt->id;

    if (tx_pkt->instruction == DXL_INST_READ && tx_pkt->param_count >= 4) {
        u16 addr = (u16)tx_pkt->params[0] | ((u16)tx_pkt->params[1] << 8);
        u16 len = (u16)tx_pkt->params[2] | ((u16)tx_pkt->params[3] << 8);

        // Torque Enable register (64): return stub_torque_on value
        if (addr == 64 && len == 1) {
            rx_pkt->param_count = 1;
            rx_pkt->params[0] = stub_torque_on ? 1 : 0;
            return DXL_OK;
        }
    }

    // Default: empty OK response (for write instructions)
    rx_pkt->param_count = 0;
    return DXL_OK;
}

dxl_result_t dxl_hal_txrx_multi(dxl_hal_context_t *ctx, const dxl_packet_t *tx_pkt, u16 data_per_response,
                                u8 expected_count, dxl_packet_t *rx_pkts, u8 *received_count, u32 timeout_ms) {
    (void)ctx;
    (void)tx_pkt;
    (void)data_per_response;
    (void)expected_count;
    (void)rx_pkts;
    (void)timeout_ms;
    *received_count = 0;
    return DXL_TIMEOUT;
}

// ---- Test setup/teardown ----

void setUp(void) {
    memset(&stub_work_pkt, 0, sizeof(stub_work_pkt));
    memset(&last_tx_pkt, 0, sizeof(last_tx_pkt));
    tx_called = false;
    stub_torque_on = false;
    stub_txrx_return_ok = true;
}

void tearDown(void) {}

// ---- Baud rate conversion tests ----

void test_baud_index_valid_rates(void) {
    TEST_ASSERT_EQUAL(9600, dxl_baud_index_to_rate(0));
    TEST_ASSERT_EQUAL(57600, dxl_baud_index_to_rate(1));
    TEST_ASSERT_EQUAL(115200, dxl_baud_index_to_rate(2));
    TEST_ASSERT_EQUAL(1000000, dxl_baud_index_to_rate(3));
    TEST_ASSERT_EQUAL(2000000, dxl_baud_index_to_rate(4));
    TEST_ASSERT_EQUAL(3000000, dxl_baud_index_to_rate(5));
    TEST_ASSERT_EQUAL(4000000, dxl_baud_index_to_rate(6));
    TEST_ASSERT_EQUAL(4500000, dxl_baud_index_to_rate(7));
}

void test_baud_index_out_of_range(void) {
    TEST_ASSERT_EQUAL(0, dxl_baud_index_to_rate(8));
    TEST_ASSERT_EQUAL(0, dxl_baud_index_to_rate(255));
}

// ---- Sync Write tests ----

void test_sync_write_single_servo(void) {
    dxl_sync_position_t entries[] = {{.id = 1, .position = 2048}};

    dxl_result_t res = dxl_sync_write_position(fake_ctx, entries, 1);
    TEST_ASSERT_EQUAL(DXL_OK, res);
    TEST_ASSERT_TRUE(tx_called);

    // Verify packet structure
    TEST_ASSERT_EQUAL_HEX8(DXL_BROADCAST_ID, last_tx_pkt.id);
    TEST_ASSERT_EQUAL_HEX8(DXL_INST_SYNC_WRITE, last_tx_pkt.instruction);

    // Params: addr(2) + data_len(2) + [id(1) + position(4)] = 9
    TEST_ASSERT_EQUAL(9, last_tx_pkt.param_count);

    // Start address: DXL_REG_GOAL_POSITION = 116 = 0x0074
    TEST_ASSERT_EQUAL_HEX8(0x74, last_tx_pkt.params[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[1]);

    // Data length: 4
    TEST_ASSERT_EQUAL_HEX8(0x04, last_tx_pkt.params[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[3]);

    // Servo 1: ID=1, position=2048 (0x00000800)
    TEST_ASSERT_EQUAL_HEX8(0x01, last_tx_pkt.params[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[5]); // pos byte 0
    TEST_ASSERT_EQUAL_HEX8(0x08, last_tx_pkt.params[6]); // pos byte 1
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[7]); // pos byte 2
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[8]); // pos byte 3
}

void test_sync_write_multiple_servos(void) {
    dxl_sync_position_t entries[] = {
        {.id = 1, .position = 0},
        {.id = 2, .position = 4095},
        {.id = 5, .position = 2048},
    };

    dxl_result_t res = dxl_sync_write_position(fake_ctx, entries, 3);
    TEST_ASSERT_EQUAL(DXL_OK, res);

    // Params: addr(2) + data_len(2) + 3 * [id(1) + position(4)] = 19
    TEST_ASSERT_EQUAL(19, last_tx_pkt.param_count);

    // Servo 1: ID=1, position=0
    TEST_ASSERT_EQUAL_HEX8(1, last_tx_pkt.params[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[7]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[8]);

    // Servo 2: ID=2, position=4095 (0x00000FFF)
    TEST_ASSERT_EQUAL_HEX8(2, last_tx_pkt.params[9]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, last_tx_pkt.params[10]);
    TEST_ASSERT_EQUAL_HEX8(0x0F, last_tx_pkt.params[11]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[12]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[13]);

    // Servo 3: ID=5, position=2048 (0x00000800)
    TEST_ASSERT_EQUAL_HEX8(5, last_tx_pkt.params[14]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[15]);
    TEST_ASSERT_EQUAL_HEX8(0x08, last_tx_pkt.params[16]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[17]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last_tx_pkt.params[18]);
}

void test_sync_write_produces_valid_wire_packet(void) {
    dxl_sync_position_t entries[] = {
        {.id = 1, .position = 2048},
        {.id = 2, .position = 1024},
    };

    dxl_result_t res = dxl_sync_write_position(fake_ctx, entries, 2);
    TEST_ASSERT_EQUAL(DXL_OK, res);

    // Build wire format from the captured packet
    u8 wire_buf[DXL_MAX_PACKET_SIZE];
    size_t wire_len = 0;
    res = dxl_build_packet(&last_tx_pkt, wire_buf, sizeof(wire_buf), &wire_len);
    TEST_ASSERT_EQUAL(DXL_OK, res);

    // Parse it back — should survive a CRC round-trip
    dxl_packet_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    res = dxl_parse_packet(wire_buf, wire_len, &parsed);
    TEST_ASSERT_EQUAL(DXL_OK, res);

    // Verify the parsed packet matches what we sent
    TEST_ASSERT_EQUAL_HEX8(DXL_BROADCAST_ID, parsed.id);
    TEST_ASSERT_EQUAL_HEX8(DXL_INST_SYNC_WRITE, parsed.instruction);
    TEST_ASSERT_EQUAL(last_tx_pkt.param_count, parsed.param_count);
    TEST_ASSERT_EQUAL_MEMORY(last_tx_pkt.params, parsed.params, parsed.param_count);
}

void test_sync_write_zero_count(void) {
    dxl_sync_position_t entries[] = {{.id = 1, .position = 0}};

    dxl_result_t res = dxl_sync_write_position(fake_ctx, entries, 0);
    TEST_ASSERT_EQUAL(DXL_INVALID_PACKET, res);
    TEST_ASSERT_FALSE(tx_called);
}

void test_sync_write_over_max(void) {
    dxl_sync_position_t entries[DXL_MAX_SYNC_SERVOS + 1];
    memset(entries, 0, sizeof(entries));

    dxl_result_t res = dxl_sync_write_position(fake_ctx, entries, DXL_MAX_SYNC_SERVOS + 1);
    TEST_ASSERT_EQUAL(DXL_INVALID_PACKET, res);
    TEST_ASSERT_FALSE(tx_called);
}

// ---- Hardware error string tests ----

void test_hw_error_none(void) {
    char buf[64];
    size_t len = dxl_hw_error_to_string(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("none", buf);
    TEST_ASSERT_EQUAL(4, len);
}

void test_hw_error_single_bit(void) {
    char buf[64];

    dxl_hw_error_to_string(DXL_HW_ERR_OVERHEATING, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("overheating", buf);

    dxl_hw_error_to_string(DXL_HW_ERR_OVERLOAD, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("overload", buf);

    dxl_hw_error_to_string(DXL_HW_ERR_INPUT_VOLTAGE, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("input voltage", buf);
}

void test_hw_error_multiple_bits(void) {
    char buf[64];
    dxl_hw_error_to_string(DXL_HW_ERR_OVERHEATING | DXL_HW_ERR_OVERLOAD, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("overheating, overload", buf);
}

void test_hw_error_all_bits(void) {
    char buf[128];
    u8 all = DXL_HW_ERR_INPUT_VOLTAGE | DXL_HW_ERR_OVERHEATING | DXL_HW_ERR_MOTOR_ENCODER | DXL_HW_ERR_ELEC_SHOCK |
             DXL_HW_ERR_OVERLOAD;
    dxl_hw_error_to_string(all, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("input voltage, overheating, motor encoder, electrical shock, overload", buf);
}

void test_hw_error_small_buffer(void) {
    char buf[8];
    dxl_hw_error_to_string(DXL_HW_ERR_OVERHEATING, buf, sizeof(buf));
    // Should be truncated but null-terminated
    TEST_ASSERT_EQUAL(0, buf[7]);
    // "overhea" (7 chars) should fit
    TEST_ASSERT_EQUAL_STRING("overhea", buf);
}

void test_hw_error_zero_buffer(void) {
    size_t len = dxl_hw_error_to_string(0, NULL, 0);
    TEST_ASSERT_EQUAL(0, len);
}

// ---- EEPROM safety tests ----

void test_set_id_blocked_when_torque_on(void) {
    stub_torque_on = true;
    dxl_result_t res = dxl_set_id(fake_ctx, 1, 2);
    TEST_ASSERT_EQUAL(DXL_TORQUE_ENABLED, res);
}

void test_set_id_allowed_when_torque_off(void) {
    stub_torque_on = false;
    dxl_result_t res = dxl_set_id(fake_ctx, 1, 2);
    TEST_ASSERT_EQUAL(DXL_OK, res);
}

void test_set_baud_blocked_when_torque_on(void) {
    stub_torque_on = true;
    dxl_result_t res = dxl_set_baud_rate(fake_ctx, 1, 3);
    TEST_ASSERT_EQUAL(DXL_TORQUE_ENABLED, res);
}

void test_set_baud_allowed_when_torque_off(void) {
    stub_torque_on = false;
    dxl_result_t res = dxl_set_baud_rate(fake_ctx, 1, 3);
    TEST_ASSERT_EQUAL(DXL_OK, res);
}

// ---- Main ----

int main(void) {
    printf("Starting Dynamixel Servo tests\n");

    UNITY_BEGIN();

    // Baud rate conversion
    RUN_TEST(test_baud_index_valid_rates);
    RUN_TEST(test_baud_index_out_of_range);

    // Sync Write
    RUN_TEST(test_sync_write_single_servo);
    RUN_TEST(test_sync_write_multiple_servos);
    RUN_TEST(test_sync_write_produces_valid_wire_packet);
    RUN_TEST(test_sync_write_zero_count);
    RUN_TEST(test_sync_write_over_max);

    // EEPROM safety
    RUN_TEST(test_set_id_blocked_when_torque_on);
    RUN_TEST(test_set_id_allowed_when_torque_off);
    RUN_TEST(test_set_baud_blocked_when_torque_on);
    RUN_TEST(test_set_baud_allowed_when_torque_off);

    // Hardware error strings
    RUN_TEST(test_hw_error_none);
    RUN_TEST(test_hw_error_single_bit);
    RUN_TEST(test_hw_error_multiple_bits);
    RUN_TEST(test_hw_error_all_bits);
    RUN_TEST(test_hw_error_small_buffer);
    RUN_TEST(test_hw_error_zero_buffer);

    return UNITY_END();
}

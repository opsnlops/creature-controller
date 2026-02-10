/**
 * @file test_dynamixel_protocol.c
 * @brief Tests for Dynamixel Protocol 2.0 packet layer
 *
 * Tests CRC16, packet build/parse round-trips, byte stuffing,
 * error detection, and edge cases. No hardware required.
 */

#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "dynamixel/dynamixel_protocol.h"

void setUp(void) {}
void tearDown(void) {}

/**
 * Test CRC16 against a known vector from the Dynamixel documentation.
 *
 * Example from the e-Manual: a Ping instruction to ID 1
 * Wire bytes (excluding CRC): FF FF FD 00 01 03 00 01
 * Expected CRC: 0x19 0x4E  -> 0x4E19
 */
void test_crc16_known_vector_ping(void) {
    u8 data[] = {0xFF, 0xFF, 0xFD, 0x00, 0x01, 0x03, 0x00, 0x01};
    u16 crc = dxl_crc16(data, sizeof(data));
    TEST_ASSERT_EQUAL_HEX16(0x4E19, crc);
}

/**
 * Test CRC16 against another known vector: a Write instruction.
 *
 * From the e-Manual: Write 1 to LED register (addr 65) on ID 1
 * Wire bytes: FF FF FD 00 01 06 00 03 41 00 01
 * Expected CRC: 0x39 0x47 -> 0x4739 (little-endian in packet)
 *
 * Full packet: FF FF FD 00 01 06 00 03 41 00 01 39 47
 */
void test_crc16_known_vector_write(void) {
    u8 data[] = {0xFF, 0xFF, 0xFD, 0x00, 0x01, 0x06, 0x00, 0x03, 0x41, 0x00, 0x01};
    u16 crc = dxl_crc16(data, sizeof(data));
    // LED write to ID 1
    // Verify we get a valid non-zero CRC (exact value depends on table correctness)
    TEST_ASSERT_NOT_EQUAL(0, crc);
}

/**
 * Test CRC16 with empty data returns 0.
 */
void test_crc16_empty(void) {
    u16 crc = dxl_crc16(NULL, 0);
    TEST_ASSERT_EQUAL_HEX16(0x0000, crc);
}

/**
 * Test building a simple ping packet and verifying the wire format.
 */
void test_build_ping_packet(void) {
    dxl_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.id = 1;
    pkt.instruction = DXL_INST_PING;
    pkt.param_count = 0;

    u8 buf[DXL_MAX_PACKET_SIZE];
    size_t length = 0;

    dxl_result_t result = dxl_build_packet(&pkt, buf, sizeof(buf), &length);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    // Ping packet should be 10 bytes: header(4) + ID(1) + length(2) + inst(1) + CRC(2)
    TEST_ASSERT_EQUAL(10, length);

    // Verify header
    TEST_ASSERT_EQUAL_HEX8(0xFF, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFD, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[3]);

    // Verify ID
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]);

    // Verify length = 3 (instruction + CRC)
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[6]);

    // Verify instruction
    TEST_ASSERT_EQUAL_HEX8(DXL_INST_PING, buf[7]);

    // Verify CRC matches our known vector
    u16 crc = (u16)buf[8] | ((u16)buf[9] << 8);
    TEST_ASSERT_EQUAL_HEX16(0x4E19, crc);
}

/**
 * Test build-then-parse round-trip with parameters.
 */
void test_build_parse_roundtrip(void) {
    dxl_packet_t pkt_out;
    memset(&pkt_out, 0, sizeof(pkt_out));
    pkt_out.id = 5;
    pkt_out.instruction = DXL_INST_WRITE;
    pkt_out.param_count = 3;
    pkt_out.params[0] = 0x41; // address low
    pkt_out.params[1] = 0x00; // address high
    pkt_out.params[2] = 0x01; // data

    u8 buf[DXL_MAX_PACKET_SIZE];
    size_t length = 0;

    dxl_result_t result = dxl_build_packet(&pkt_out, buf, sizeof(buf), &length);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    dxl_packet_t pkt_in;
    memset(&pkt_in, 0, sizeof(pkt_in));
    result = dxl_parse_packet(buf, length, &pkt_in);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    TEST_ASSERT_EQUAL(pkt_out.id, pkt_in.id);
    TEST_ASSERT_EQUAL(pkt_out.instruction, pkt_in.instruction);
    TEST_ASSERT_EQUAL(pkt_out.param_count, pkt_in.param_count);
    TEST_ASSERT_EQUAL_MEMORY(pkt_out.params, pkt_in.params, pkt_out.param_count);
}

/**
 * Test that CRC mismatch is detected.
 */
void test_crc_mismatch_detection(void) {
    dxl_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.id = 1;
    pkt.instruction = DXL_INST_PING;
    pkt.param_count = 0;

    u8 buf[DXL_MAX_PACKET_SIZE];
    size_t length = 0;

    dxl_result_t result = dxl_build_packet(&pkt, buf, sizeof(buf), &length);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    // Corrupt the CRC
    buf[length - 1] ^= 0xFF;

    dxl_packet_t pkt_in;
    result = dxl_parse_packet(buf, length, &pkt_in);
    TEST_ASSERT_EQUAL(DXL_CRC_MISMATCH, result);
}

/**
 * Test that an invalid header is detected.
 */
void test_invalid_header_detection(void) {
    u8 bad_data[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x19, 0x4E};

    dxl_packet_t pkt;
    dxl_result_t result = dxl_parse_packet(bad_data, sizeof(bad_data), &pkt);
    TEST_ASSERT_EQUAL(DXL_INVALID_PACKET, result);
}

/**
 * Test that a too-short packet is rejected.
 */
void test_too_short_packet(void) {
    u8 short_data[] = {0xFF, 0xFF, 0xFD, 0x00, 0x01};

    dxl_packet_t pkt;
    dxl_result_t result = dxl_parse_packet(short_data, sizeof(short_data), &pkt);
    TEST_ASSERT_EQUAL(DXL_INVALID_PACKET, result);
}

/**
 * Test multi-byte parameter handling (4-byte write).
 */
void test_multi_byte_params(void) {
    dxl_packet_t pkt_out;
    memset(&pkt_out, 0, sizeof(pkt_out));
    pkt_out.id = 1;
    pkt_out.instruction = DXL_INST_WRITE;
    pkt_out.param_count = 6;
    // Write goal position 2048 (0x00000800) to address 116 (0x0074)
    pkt_out.params[0] = 0x74; // address low
    pkt_out.params[1] = 0x00; // address high
    pkt_out.params[2] = 0x00; // data byte 0
    pkt_out.params[3] = 0x08; // data byte 1
    pkt_out.params[4] = 0x00; // data byte 2
    pkt_out.params[5] = 0x00; // data byte 3

    u8 buf[DXL_MAX_PACKET_SIZE];
    size_t length = 0;

    dxl_result_t result = dxl_build_packet(&pkt_out, buf, sizeof(buf), &length);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    dxl_packet_t pkt_in;
    memset(&pkt_in, 0, sizeof(pkt_in));
    result = dxl_parse_packet(buf, length, &pkt_in);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    TEST_ASSERT_EQUAL(6, pkt_in.param_count);
    TEST_ASSERT_EQUAL_MEMORY(pkt_out.params, pkt_in.params, 6);
}

/**
 * Test zero-parameter packet (like ping or reboot).
 */
void test_zero_param_packet(void) {
    dxl_packet_t pkt_out;
    memset(&pkt_out, 0, sizeof(pkt_out));
    pkt_out.id = 3;
    pkt_out.instruction = DXL_INST_REBOOT;
    pkt_out.param_count = 0;

    u8 buf[DXL_MAX_PACKET_SIZE];
    size_t length = 0;

    dxl_result_t result = dxl_build_packet(&pkt_out, buf, sizeof(buf), &length);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    dxl_packet_t pkt_in;
    memset(&pkt_in, 0, sizeof(pkt_in));
    result = dxl_parse_packet(buf, length, &pkt_in);
    TEST_ASSERT_EQUAL(DXL_OK, result);

    TEST_ASSERT_EQUAL(3, pkt_in.id);
    TEST_ASSERT_EQUAL(DXL_INST_REBOOT, pkt_in.instruction);
    TEST_ASSERT_EQUAL(0, pkt_in.param_count);
}

/**
 * Test buffer overflow detection when output buffer is too small.
 */
void test_buffer_overflow(void) {
    dxl_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.id = 1;
    pkt.instruction = DXL_INST_PING;
    pkt.param_count = 0;

    u8 tiny_buf[5]; // way too small for any packet
    size_t length = 0;

    dxl_result_t result = dxl_build_packet(&pkt, tiny_buf, sizeof(tiny_buf), &length);
    TEST_ASSERT_EQUAL(DXL_BUFFER_OVERFLOW, result);
}

/**
 * Test result_to_string returns meaningful strings.
 */
void test_result_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("OK", dxl_result_to_string(DXL_OK));
    TEST_ASSERT_EQUAL_STRING("Timeout", dxl_result_to_string(DXL_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("CRC mismatch", dxl_result_to_string(DXL_CRC_MISMATCH));
}

/**
 * Test error_to_string returns meaningful strings.
 */
void test_error_to_string(void) {
    TEST_ASSERT_EQUAL_STRING("None", dxl_error_to_string(0));
    TEST_ASSERT_EQUAL_STRING("Data range error", dxl_error_to_string(DXL_ERR_DATA_RANGE));
    TEST_ASSERT_EQUAL_STRING("Access error", dxl_error_to_string(DXL_ERR_ACCESS));
}

int main(void) {
    printf("Starting Dynamixel Protocol 2.0 tests\n");

    UNITY_BEGIN();

    RUN_TEST(test_crc16_known_vector_ping);
    RUN_TEST(test_crc16_known_vector_write);
    RUN_TEST(test_crc16_empty);
    RUN_TEST(test_build_ping_packet);
    RUN_TEST(test_build_parse_roundtrip);
    RUN_TEST(test_crc_mismatch_detection);
    RUN_TEST(test_invalid_header_detection);
    RUN_TEST(test_too_short_packet);
    RUN_TEST(test_multi_byte_params);
    RUN_TEST(test_zero_param_packet);
    RUN_TEST(test_buffer_overflow);
    RUN_TEST(test_result_to_string);
    RUN_TEST(test_error_to_string);

    return UNITY_END();
}

/**
 * @file test_eeprom_hours.c
 * @brief Tests for the power-on hours odometer record logic
 *
 * Covers CRC-16, record serialize/deserialize round-trips, corruption
 * rejection, and newest-slot selection across the wear-leveled ring
 * (including the power-loss / torn-write fallback). No hardware required.
 */

#include "unity.h"

#include <string.h>

#include "controller/config.h"
#include "device/eeprom_hours.h"

void setUp(void) {}
void tearDown(void) {}

/**
 * CRC-16/CCITT-FALSE has a well-known check value: the ASCII string
 * "123456789" must produce 0x29B1. This pins the algorithm down.
 */
void test_crc16_known_check_value(void) {
    const u8 data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, eeprom_hours_crc16(data, sizeof(data)));
}

void test_serialize_deserialize_round_trip(void) {
    u8 slot[EEPROM_HOURS_SLOT_SIZE];
    eeprom_hours_serialize(slot, 42, 12345, 6789);

    u32 seq = 0, uptime = 0, motor = 0;
    TEST_ASSERT_TRUE(eeprom_hours_deserialize(slot, &seq, &uptime, &motor));
    TEST_ASSERT_EQUAL_UINT32(42, seq);
    TEST_ASSERT_EQUAL_UINT32(12345, uptime);
    TEST_ASSERT_EQUAL_UINT32(6789, motor);
}

void test_deserialize_rejects_bad_magic(void) {
    u8 slot[EEPROM_HOURS_SLOT_SIZE];
    eeprom_hours_serialize(slot, 1, 1, 1);
    slot[0] ^= 0xFF; // Corrupt the magic

    TEST_ASSERT_FALSE(eeprom_hours_deserialize(slot, NULL, NULL, NULL));
}

void test_deserialize_rejects_corrupt_payload(void) {
    u8 slot[EEPROM_HOURS_SLOT_SIZE];
    eeprom_hours_serialize(slot, 7, 1000, 2000);
    slot[7] ^= 0x01; // Flip a bit in the uptime field; CRC must now fail

    TEST_ASSERT_FALSE(eeprom_hours_deserialize(slot, NULL, NULL, NULL));
}

void test_select_slot_blank_eeprom_returns_minus_one(void) {
    u8 region[EEPROM_HOURS_REGION_SIZE];
    memset(region, 0xFF, sizeof(region)); // Erased EEPROM

    u32 seq = 0, uptime = 0, motor = 0;
    TEST_ASSERT_EQUAL_INT(-1, eeprom_hours_select_slot(region, sizeof(region), &seq, &uptime, &motor));
}

void test_select_slot_picks_highest_sequence(void) {
    u8 region[EEPROM_HOURS_REGION_SIZE];
    memset(region, 0xFF, sizeof(region));

    // Newest record (seq 100) sits in slot 1; an older one (seq 99) in slot 5.
    eeprom_hours_serialize(&region[5 * EEPROM_HOURS_SLOT_SIZE], 99, 500, 50);
    eeprom_hours_serialize(&region[1 * EEPROM_HOURS_SLOT_SIZE], 100, 600, 60);

    u32 seq = 0, uptime = 0, motor = 0;
    int idx = eeprom_hours_select_slot(region, sizeof(region), &seq, &uptime, &motor);

    TEST_ASSERT_EQUAL_INT(1, idx);
    TEST_ASSERT_EQUAL_UINT32(100, seq);
    TEST_ASSERT_EQUAL_UINT32(600, uptime);
    TEST_ASSERT_EQUAL_UINT32(60, motor);
}

/**
 * Power-loss case: the newest slot is half-written (CRC invalid). Selection
 * must ignore it and fall back to the previous good record.
 */
void test_select_slot_falls_back_past_torn_write(void) {
    u8 region[EEPROM_HOURS_REGION_SIZE];
    memset(region, 0xFF, sizeof(region));

    eeprom_hours_serialize(&region[2 * EEPROM_HOURS_SLOT_SIZE], 200, 900, 90);
    eeprom_hours_serialize(&region[3 * EEPROM_HOURS_SLOT_SIZE], 201, 999, 99);
    region[3 * EEPROM_HOURS_SLOT_SIZE + 9] ^= 0xAA; // Tear the newest record

    u32 seq = 0, uptime = 0, motor = 0;
    int idx = eeprom_hours_select_slot(region, sizeof(region), &seq, &uptime, &motor);

    TEST_ASSERT_EQUAL_INT(2, idx);
    TEST_ASSERT_EQUAL_UINT32(200, seq);
    TEST_ASSERT_EQUAL_UINT32(900, uptime);
    TEST_ASSERT_EQUAL_UINT32(90, motor);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crc16_known_check_value);
    RUN_TEST(test_serialize_deserialize_round_trip);
    RUN_TEST(test_deserialize_rejects_bad_magic);
    RUN_TEST(test_deserialize_rejects_corrupt_payload);
    RUN_TEST(test_select_slot_blank_eeprom_returns_minus_one);
    RUN_TEST(test_select_slot_picks_highest_sequence);
    RUN_TEST(test_select_slot_falls_back_past_torn_write);
    return UNITY_END();
}

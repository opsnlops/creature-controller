
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "types.h"

/*
 * Power-on hours odometer.
 *
 * Tracks two lifetime counters for a creature and persists them to the I2C
 * EEPROM: total controller uptime, and the time the motor power rail has been
 * energized. Storage is a wear-leveled ring of fixed-size records at the top
 * of the EEPROM (see EEPROM_HOURS_* in config.h).
 *
 * On-disk record layout (EEPROM_HOURS_SLOT_SIZE bytes, all big-endian to match
 * the rest of the EEPROM):
 *
 *   offset  size  field
 *   ------  ----  --------------------------------------------------
 *      0     2    magic        (EEPROM_HOURS_MAGIC)
 *      2     4    seq          (monotonic write sequence; highest wins)
 *      6     4    uptime_min   (total controller uptime, minutes)
 *     10     4    motor_min    (total motor-powered time, minutes)
 *     14     2    crc16        (CRC-16/CCITT over bytes 0..13)
 *
 * The functions below are pure: no hardware, no FreeRTOS, fully unit-testable
 * on the host. The runtime glue that talks to the EEPROM and FreeRTOS lives in
 * eeprom.c.
 */

/** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). */
u16 eeprom_hours_crc16(const u8 *data, size_t len);

/** Serialize a record into a slot buffer of EEPROM_HOURS_SLOT_SIZE bytes. */
void eeprom_hours_serialize(u8 *slot, u32 seq, u32 uptime_min, u32 motor_min);

/**
 * Validate and decode a slot. Returns false (and leaves outputs untouched) if
 * the magic or CRC does not match. Output pointers may be NULL.
 */
bool eeprom_hours_deserialize(const u8 *slot, u32 *seq, u32 *uptime_min, u32 *motor_min);

/**
 * Scan a region buffer and return the index of the newest valid record (the
 * one with the highest sequence number), or -1 if none is valid (e.g. a blank
 * 0xFF EEPROM). The newest record's fields are written to the output pointers.
 */
int eeprom_hours_select_slot(const u8 *region, size_t region_len, u32 *seq, u32 *uptime_min, u32 *motor_min);

/* -- Runtime odometer (implemented in eeprom.c) -- */

/** Restore the baseline counters from the EEPROM. Call once at boot. */
void eeprom_hours_init(void);

/** Create and start the periodic persist timer. Call after init. */
void eeprom_hours_start(void);

/**
 * Feed the latest motor-power-rail voltage to the odometer. Called from the
 * i2c sensor read callback every cycle. Cheap and non-blocking: it only
 * integrates elapsed time, it never touches the bus.
 */
void eeprom_hours_motor_sample(float motor_voltage);

/** Current totals in whole hours, for status reporting. */
u32 eeprom_hours_get_uptime_hours(void);
u32 eeprom_hours_get_motor_hours(void);


/**
 * @file eeprom_hours.c
 * @brief Pure record logic for the power-on hours odometer.
 *
 * No hardware, no FreeRTOS - this is the part that is unit tested on the host.
 * The runtime glue (EEPROM I/O, the persist timer, motor-voltage integration)
 * lives in eeprom.c.
 */

#include <string.h>

#include "controller/config.h"

#include "eeprom_hours.h"
#include "types.h"

u16 eeprom_hours_crc16(const u8 *data, size_t len) {
    u16 crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (u16)((u16)data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (u16)((crc << 1) ^ 0x1021) : (u16)(crc << 1);
        }
    }
    return crc;
}

static void put_u16(u8 *p, u16 v) {
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static void put_u32(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static u16 get_u16(const u8 *p) { return (u16)(((u16)p[0] << 8) | p[1]); }

static u32 get_u32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3]; }

void eeprom_hours_serialize(u8 *slot, u32 seq, u32 uptime_min, u32 motor_min) {
    memset(slot, 0, EEPROM_HOURS_SLOT_SIZE);
    put_u16(&slot[0], EEPROM_HOURS_MAGIC);
    put_u32(&slot[2], seq);
    put_u32(&slot[6], uptime_min);
    put_u32(&slot[10], motor_min);
    put_u16(&slot[14], eeprom_hours_crc16(slot, 14));
}

bool eeprom_hours_deserialize(const u8 *slot, u32 *seq, u32 *uptime_min, u32 *motor_min) {
    if (get_u16(&slot[0]) != EEPROM_HOURS_MAGIC) {
        return false;
    }
    if (get_u16(&slot[14]) != eeprom_hours_crc16(slot, 14)) {
        return false;
    }
    if (seq != NULL) {
        *seq = get_u32(&slot[2]);
    }
    if (uptime_min != NULL) {
        *uptime_min = get_u32(&slot[6]);
    }
    if (motor_min != NULL) {
        *motor_min = get_u32(&slot[10]);
    }
    return true;
}

int eeprom_hours_select_slot(const u8 *region, size_t region_len, u32 *seq, u32 *uptime_min, u32 *motor_min) {
    int best = -1;
    u32 best_seq = 0;

    size_t count = region_len / EEPROM_HOURS_SLOT_SIZE;
    if (count > EEPROM_HOURS_SLOT_COUNT) {
        count = EEPROM_HOURS_SLOT_COUNT;
    }

    for (size_t i = 0; i < count; i++) {
        const u8 *slot = &region[i * EEPROM_HOURS_SLOT_SIZE];
        u32 s = 0;
        u32 u = 0;
        u32 m = 0;

        if (!eeprom_hours_deserialize(slot, &s, &u, &m)) {
            continue;
        }

        // Sequence is monotonic and (at one write per 15 minutes) will not wrap
        // for ~120,000 years, so a plain max is the newest record.
        if (best < 0 || s > best_seq) {
            best = (int)i;
            best_seq = s;
            if (seq != NULL) {
                *seq = s;
            }
            if (uptime_min != NULL) {
                *uptime_min = u;
            }
            if (motor_min != NULL) {
                *motor_min = m;
            }
        }
    }

    return best;
}


#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "controller/config.h"
#include "logging/logging.h"

#include "eeprom.h"
#include "eeprom_hours.h"

extern uint16_t usb_pid;
extern uint16_t usb_vid;
extern uint16_t usb_version;
extern char usb_serial[16];
extern char usb_product[16];
extern char usb_manufacturer[32];

extern uint8_t configured_logging_level;

void dump_hex(const uint8_t *data, size_t len) {
    debug("EEPROM Raw Dump:");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

/**
 * Configure the I2C bus for the EEPROM
 */
void eeprom_setup_i2c() {

    debug("Configuring EEPROM I2C");

    gpio_set_function(EEPROM_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(EEPROM_SCL_PIN, GPIO_FUNC_I2C);

    i2c_init(EEPROM_I2C_BUS, 100 * 1000); // Nice and slow at 100kHz

    gpio_pull_up(EEPROM_SDA_PIN);
    gpio_pull_up(EEPROM_SCL_PIN);

    debug("I2C configured at %uHz", 100000);
}

/**
 * Read the EEPROM and configure the USB device
 */
void read_eeprom_and_configure() {
    info("Reading EEPROM and configuring");

    u16 data_size = 60;

    // Buffer to hold the data
    u8 *eeprom_data = pvPortMalloc(data_size);
    memset(eeprom_data, '\0', data_size);

    // Read the EEPROM
    eeprom_read(EEPROM_I2C_BUS, EEPROM_I2C_ADDR, 0, eeprom_data, data_size);

    // dump_hex(eeprom_data, data_size);

    parse_eeprom_data(eeprom_data, data_size);

    //  Release the memory
    vPortFree(eeprom_data);
}

/**
 * Read data from the EEPROM
 */
void eeprom_read(i2c_inst_t *i2c, u8 eeprom_addr, u16 mem_addr, u8 *data, size_t len) {
    while (len > 0) {
        size_t read_len = len > EEPROM_PAGE_SIZE ? EEPROM_PAGE_SIZE : len;

        debug("reading %u bytes starting at address 0x%02X", read_len, mem_addr);

        // Write the memory address we want to start reading from
        u8 addr_buffer[2] = {(u8)((mem_addr >> 8) & 0xFF), (u8)(mem_addr & 0xFF)};
        i2c_write_blocking(i2c, eeprom_addr, addr_buffer, 2, true); // true means keep the bus active

        // Read the data back
        i2c_read_blocking(i2c, eeprom_addr, data, read_len, false); // false means release the bus after read

        // Move to the next page
        data += read_len;
        mem_addr += read_len;
        len -= read_len;
    }
}

/**
 * Parse the EEPROM data
 *
 * @param data the data read in from the EEPROM
 * @param len Length of the data
 * @return 0 if successful or -1 if not
 */
int parse_eeprom_data(const u8 *data, size_t len) {
    size_t offset = 0;

    // Check that we have enough data for the fixed header.
    if (len < MAGIC_WORD_SIZE + 2 + 2 + 2 + 1) {
        error("Not enough data to start parsing EEPROM data!");
        return -1;
    }

    // Verify the magic bytes ("HOP!").
    if (memcmp(data, MAGIC_WORD, MAGIC_WORD_SIZE) != 0) {
        error("Magic bytes don't match – this isn't one of our EEPROMS!");
        return -1;
    }
    offset += MAGIC_WORD_SIZE;

    // Read VID (big-endian).
    usb_vid = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    // Read PID (big-endian).
    usb_pid = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    // Skip version (BCD format, big-endian) - use firmware version instead
    // usb_version = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    // Read logging level.
    configured_logging_level = data[offset++];

    // Extract strings using our helper function.
    if (extract_string(data, len, &offset, usb_serial, sizeof(usb_serial), "serial number") != 0)
        return -1;
    if (extract_string(data, len, &offset, usb_product, sizeof(usb_product), "product name") != 0)
        return -1;
    if (extract_string(data, len, &offset, usb_manufacturer, sizeof(usb_manufacturer), "manufacturer") != 0)
        return -1;

    // Print the parsed configuration – hop to it, little bunny!
    info("-- EEPROM Config Dump --");
    info(" VID: 0x%04X", usb_vid);
    info(" PID: 0x%04X", usb_pid);
    info(" Version: %d.%d (BCD: 0x%04X)", usb_version >> 8, usb_version & 0xFF, usb_version);
    info(" Logging Level: %s", log_level_to_string(configured_logging_level));
    info(" Serial Number: %s", usb_serial);
    info(" Product Name: %s", usb_product);
    info(" Manufacturer: %s", usb_manufacturer);

    // Parse any additional custom strings, if present.
    int customIndex = 0;
    while (offset < len) {
        uint8_t custom_len = data[offset++];

        // If we hit an erased EEPROM byte, assume no more valid custom strings.
        if (custom_len == 0xFF) {
            debug("Custom string %d is empty (erased EEPROM).", customIndex);
            continue;
        }

        if (offset + custom_len > len) {
            debug("Not enough data for custom string %d!", customIndex);
            break;
        }
        char customStr[256] = {0};
        if (custom_len >= sizeof(customStr)) {
            error("Custom string %d is too long!", customIndex);
            return -1;
        }
        memcpy(customStr, &data[offset], custom_len);
        customStr[custom_len] = '\0';
        offset += custom_len;
        debug("Custom String %d: %s", customIndex, customStr);
        customIndex++;
    }

    return 0;
}

/**
 * Extract a string from the EEPROM data
 *
 * @param data
 * @param len
 * @param offset
 * @param buffer
 * @param bufsize
 * @param fieldName
 * @return
 */
int extract_string(const u8 *data, size_t len, size_t *offset, char *buffer, size_t bufsize, const char *fieldName) {
    if (*offset >= len) {
        error("No data for %s length!", fieldName);
        return -1;
    }

    u8 str_len = data[*offset];

    // If we hit an erased EEPROM cell, treat it as empty.
    if (str_len == 0xFF) {
        debug("%s field is empty (erased EEPROM)", fieldName);
        buffer[0] = '\0';
        (*offset)++;
        return 0;
    }

    (*offset)++; // Move past the length byte.

    if (*offset + str_len > len) {
        error("Not enough data for %s!", fieldName);
        return -1;
    }

    if (str_len >= bufsize) {
        error("%s too long to fit in buffer!", fieldName);
        return -1;
    }

    memcpy(buffer, &data[*offset], str_len);
    buffer[str_len] = '\0';
    *offset += str_len;

    return 0;
}

/* ------------------------------------------------------------------------- *
 *  Power-on hours odometer (runtime glue)
 *
 *  The pure record logic lives in eeprom_hours.c. This is the part that talks
 *  to the bus and to FreeRTOS:
 *
 *    - eeprom_write()              page-aware EEPROM write
 *    - eeprom_hours_init()         boot: restore the baseline counters
 *    - eeprom_hours_start()        start the 15-minute persist timer
 *    - eeprom_hours_motor_sample() integrate motor-powered time
 *
 *  Both the persist timer and the sensor callback that calls
 *  eeprom_hours_motor_sample() run in the FreeRTOS timer-daemon task, so they
 *  are mutually serialized - no locks are needed, and the EEPROM stays a
 *  single-owner bus exactly as the sensor subsystem requires.
 * ------------------------------------------------------------------------- */

#if USE_EEPROM

/**
 * Write to the EEPROM, never letting a single page write cross a hardware
 * page boundary. The chip needs an internal write cycle (~5 ms) before it
 * accepts the next page, so we wait *between* pages - but never after the
 * last one. The odometer writes a single 16-byte slot every 15 minutes, so
 * in practice this never spans a page and never waits at all.
 */
void eeprom_write(i2c_inst_t *i2c, u8 eeprom_addr, u16 mem_addr, const u8 *data, size_t len) {
    while (len > 0) {
        size_t page_remaining = EEPROM_PAGE_SIZE - (mem_addr % EEPROM_PAGE_SIZE);
        size_t write_len = len < page_remaining ? len : page_remaining;

        u8 buffer[EEPROM_PAGE_SIZE + 2]; // 2 bytes for the memory address
        buffer[0] = (u8)((mem_addr >> 8) & 0xFF);
        buffer[1] = (u8)(mem_addr & 0xFF);
        memcpy(&buffer[2], data, write_len);

        i2c_write_blocking(i2c, eeprom_addr, buffer, write_len + 2, false);

        data += write_len;
        mem_addr += write_len;
        len -= write_len;

        if (len > 0) {
            vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_CYCLE_MS));
        }
    }
}

// Baselines restored from the EEPROM at boot.
static u32 baseline_uptime_min = 0;
static u32 baseline_motor_min = 0;

// Ring position of the most recently written (or restored) record.
static u32 last_seq = 0;
static u32 last_slot = EEPROM_HOURS_SLOT_COUNT - 1;

// Motor-powered time accumulated since boot, integrated from the sensor loop.
static u64 motor_on_accum_us = 0;
static absolute_time_t last_motor_sample;
static bool motor_sample_primed = false;

static u32 current_uptime_minutes(void) {
    u64 us = to_us_since_boot(get_absolute_time());
    return baseline_uptime_min + (u32)(us / 60000000ULL);
}

static u32 current_motor_minutes(void) { return baseline_motor_min + (u32)(motor_on_accum_us / 60000000ULL); }

void eeprom_hours_init(void) {
    static u8 region[EEPROM_HOURS_REGION_SIZE];

    eeprom_read(EEPROM_I2C_BUS, EEPROM_I2C_ADDR, EEPROM_HOURS_REGION_ADDR, region, EEPROM_HOURS_REGION_SIZE);

    u32 seq = 0;
    u32 uptime_min = 0;
    u32 motor_min = 0;
    int slot = eeprom_hours_select_slot(region, EEPROM_HOURS_REGION_SIZE, &seq, &uptime_min, &motor_min);

    if (slot < 0) {
        info("No valid power-on hours record found - starting the odometer at zero");
        baseline_uptime_min = 0;
        baseline_motor_min = 0;
        last_seq = 0;
        last_slot = EEPROM_HOURS_SLOT_COUNT - 1;
    } else {
        baseline_uptime_min = uptime_min;
        baseline_motor_min = motor_min;
        last_seq = seq;
        last_slot = (u32)slot;
        info("Power-on hours odometer restored: uptime %lu h, motor %lu h (slot %d, seq %lu)",
             (unsigned long)(uptime_min / 60), (unsigned long)(motor_min / 60), slot, (unsigned long)seq);
    }

    motor_on_accum_us = 0;
    motor_sample_primed = false;
}

static void eeprom_hours_persist(void) {
    u32 uptime_min = current_uptime_minutes();
    u32 motor_min = current_motor_minutes();
    u32 seq = last_seq + 1;
    u32 slot = (last_slot + 1) % EEPROM_HOURS_SLOT_COUNT;

    u8 record[EEPROM_HOURS_SLOT_SIZE];
    eeprom_hours_serialize(record, seq, uptime_min, motor_min);

    eeprom_write(EEPROM_I2C_BUS, EEPROM_I2C_ADDR, (u16)(EEPROM_HOURS_REGION_ADDR + slot * EEPROM_HOURS_SLOT_SIZE),
                 record, EEPROM_HOURS_SLOT_SIZE);

    last_seq = seq;
    last_slot = slot;

    debug("Persisted odometer: uptime %lu min, motor %lu min (slot %lu, seq %lu)", (unsigned long)uptime_min,
          (unsigned long)motor_min, (unsigned long)slot, (unsigned long)seq);
}

static void eeprom_hours_timer_callback(TimerHandle_t xTimer) {
    (void)xTimer;
    eeprom_hours_persist();
}

void eeprom_hours_start(void) {
    TimerHandle_t timer = xTimerCreate("EepromHoursTimer", pdMS_TO_TICKS(EEPROM_HOURS_PERSIST_INTERVAL_MS),
                                       pdTRUE,    // Auto-reload
                                       (void *)0, // Timer ID (unused)
                                       eeprom_hours_timer_callback);

    if (timer != NULL) {
        xTimerStart(timer, 0);
        info("Power-on hours odometer started (persist every %d ms)", EEPROM_HOURS_PERSIST_INTERVAL_MS);
    } else {
        error("Failed to create the power-on hours persist timer");
    }
}

void eeprom_hours_motor_sample(float motor_voltage) {
    absolute_time_t now = get_absolute_time();

    if (!motor_sample_primed) {
        last_motor_sample = now;
        motor_sample_primed = true;
        return;
    }

    int64_t delta_us = absolute_time_diff_us(last_motor_sample, now);
    last_motor_sample = now;

    if (delta_us > 0 && motor_voltage > MOTOR_POWER_ON_VOLTAGE_THRESHOLD) {
        motor_on_accum_us += (u64)delta_us;
    }
}

u32 eeprom_hours_get_uptime_hours(void) { return current_uptime_minutes() / 60; }

u32 eeprom_hours_get_motor_hours(void) { return current_motor_minutes() / 60; }

#else // !USE_EEPROM

// Stubs so every target that compiles this file links cleanly even when the
// EEPROM (and therefore the odometer) is disabled.
void eeprom_write(i2c_inst_t *i2c, u8 eeprom_addr, u16 mem_addr, const u8 *data, size_t len) {
    (void)i2c;
    (void)eeprom_addr;
    (void)mem_addr;
    (void)data;
    (void)len;
}
void eeprom_hours_init(void) {}
void eeprom_hours_start(void) {}
void eeprom_hours_motor_sample(float motor_voltage) { (void)motor_voltage; }
u32 eeprom_hours_get_uptime_hours(void) { return 0; }
u32 eeprom_hours_get_motor_hours(void) { return 0; }

#endif // USE_EEPROM

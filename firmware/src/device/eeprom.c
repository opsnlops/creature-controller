
#include <FreeRTOS.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"


#include "controller/config.h"
#include "logging/logging.h"


#include "eeprom.h"


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

    i2c_init(EEPROM_I2C_BUS, 100 * 1000);   // Nice and slow at 100kHz

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
    u8* eeprom_data = pvPortMalloc(data_size);
    memset(eeprom_data, '\0', data_size);


    // Read the EEPROM
    eeprom_read(EEPROM_I2C_BUS, EEPROM_I2C_ADDR, 0, eeprom_data, data_size);

    //dump_hex(eeprom_data, data_size);

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
        i2c_write_blocking(i2c, eeprom_addr, addr_buffer, 2, true);  // true means keep the bus active

        // Read the data back
        i2c_read_blocking(i2c, eeprom_addr, data, read_len, false);  // false means release the bus after read

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

    // Read version (BCD format, big-endian).
    usb_version = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    // Read logging level.
    configured_logging_level = data[offset++];

    // Extract strings using our helper function.
    if (extract_string(data, len, &offset, usb_serial , sizeof(usb_serial), "serial number") != 0)
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
int extract_string(const u8 *data, size_t len, size_t *offset,
                   char *buffer, size_t bufsize, const char *fieldName) {
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

    (*offset)++;  // Move past the length byte.

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

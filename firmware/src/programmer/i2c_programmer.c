
#include <stddef.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/i2c.h"


#include "logging/logging.h"

#include "controller/config.h"
#include "i2c_programmer.h"
#include "types.h"

// Our task handle
TaskHandle_t programming_task_handle;

// Define the EEPROM page size (check the EEPROM's datasheet)
#define EEPROM_PAGE_SIZE 64

extern u8* incoming_data_buffer;
extern u32 incomingBufferIndex;

// What state are we in?
extern enum ProgrammerState programmer_state;
extern u32 program_size;


void programmer_setup_i2c() {

    debug("Configuring I2C");

    gpio_set_function(PROGRAMMER_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PROGRAMMER_SCL_PIN, GPIO_FUNC_I2C);

    i2c_init(PROGRAMMER_I2C_BUS, 100 * 1000);   // Nice and slow at 100kHz

    gpio_pull_up(PROGRAMMER_SDA_PIN);
    gpio_pull_up(PROGRAMMER_SCL_PIN);

    debug("I2C configured at %uHz", 100000);

}

void write_incoming_buffer_to_eeprom() {

    info("Programming I2C EEPROM");
    debug("There are %u bytes to program", program_size);

    // Write the full flash array to the EEPROM
    i2c_eeprom_write(PROGRAMMER_I2C_BUS, PROGRAMMER_I2C_ADDR, 0, incoming_data_buffer, program_size);
    info("I2C EEPROM programmed");

}


void i2c_eeprom_write(i2c_inst_t *i2c, uint8_t eeprom_addr, uint16_t mem_addr, const u8 *data, size_t len) {

    debug("Writing %u bytes to EEPROM at address 0x%02X", len, eeprom_addr);

    uint32_t total_bytes_written = 0;

    while (len > 0) {
        size_t write_len = len > EEPROM_PAGE_SIZE ? EEPROM_PAGE_SIZE : len;

        total_bytes_written += write_len;

        uint8_t buffer[EEPROM_PAGE_SIZE + 2]; // 2 bytes for memory address
        buffer[0] = (mem_addr >> 8) & 0xFF;  // High byte of memory address
        buffer[1] = mem_addr & 0xFF;         // Low byte of memory address

        // Copy data to buffer
        for (size_t i = 0; i < write_len; ++i) {
            buffer[i + 2] = data[i];
        }

        // Write the page to EEPROM
        verbose("calling i2c_write_blocking");
        i2c_write_blocking(i2c, eeprom_addr, buffer, write_len + 2, false);
        verbose("i2c_write_blocking done");

        // Move to the next page
        data += write_len;
        mem_addr += write_len;
        len -= write_len;

        if(total_bytes_written % 2048 == 0) {
            debug("Wrote %u bytes to EEPROM", total_bytes_written);
        }


        // Wait for the EEPROM to complete the write cycle (typically ~5ms, but let's go slow)
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}


void print_eeprom_contents(const u8 *data, size_t len) {

    // Make sure we're not trying to print nothing
    if(data == NULL || len == 0) {
        warning("No data to print");
        return;
    }

    // Buffer to hold a line of output
    char line_buffer[140];

    verbose("--- Start of EEPROM data ---");

    for (size_t i = 0; i < len; i++) {
        if (i % 32 == 0) {
            // Start a new line with the memory address, clear the buffer first
            memset(line_buffer, 0, sizeof(line_buffer));
            snprintf(line_buffer, sizeof(line_buffer), "  0x%04zx: ", i);
        }

        // Append the next byte to the current line, checking buffer space
        snprintf(line_buffer + strlen(line_buffer), sizeof(line_buffer) - strlen(line_buffer), "%02X ", data[i]);


        // If 32 bytes have been added, or it's the end of the data, print the line
        if (i % 32 == 31 || i == len - 1) {
            verbose(line_buffer);

            // Give the debugger's UART a chance to catch up
            vTaskDelay(pdMS_TO_TICKS(15));
        }
    }

    verbose("--- End of EEPROM data ---");
}


void i2c_eeprom_read(i2c_inst_t *i2c, uint8_t eeprom_addr, uint16_t mem_addr, uint8_t *data, size_t len) {
    while (len > 0) {
        size_t read_len = len > EEPROM_PAGE_SIZE ? EEPROM_PAGE_SIZE : len;

        verbose("reading %u bytes starting at address 0x%02X", read_len, mem_addr);

        // Write the memory address we want to start reading from
        uint8_t addr_buffer[2] = {(uint8_t)((mem_addr >> 8) & 0xFF), (uint8_t)(mem_addr & 0xFF)};
        i2c_write_blocking(i2c, eeprom_addr, addr_buffer, 2, true);  // true means keep the bus active

        // Read the data back
        i2c_read_blocking(i2c, eeprom_addr, data, read_len, false);  // false means release the bus after read

        // Move to the next page
        data += read_len;
        mem_addr += read_len;
        len -= read_len;
    }
}

bool verify_eeprom_data(i2c_inst_t *i2c, uint8_t eeprom_addr, const u8 *expected_data, size_t len, char* result, size_t result_len) {

    debug("Verifying EEPROM data, length: %u", len);

    // Use the old goto trick to make cleanup easier since we're allocating memory
    bool returnVal = false;

    // Allocate a buffer to read the data back
    debug("Allocating memory for read buffer");
    vTaskDelay(pdMS_TO_TICKS(10));
    u8* read_data = (u8*)pvPortMalloc(len);    // An assert will be thrown if this fails


    if(read_data == NULL) {
        error("Failed to allocate memory for read buffer");
        snprintf(result, result_len, "ERR Failed to allocate memory for read buffer");
        goto end;
    }

    // Read back the entire EEPROM data
    debug("Reading %u bytes from EEPROM at address 0x%02X", len, eeprom_addr);
    i2c_eeprom_read(i2c, eeprom_addr, 0, read_data, len);

    debug("Read data from EEPROM");

    // If we've got logging enabled, print the EEPROM contents
#if LOGGING_LEVEL > 4
    print_eeprom_contents(read_data, len);
#endif

    // Compare the read data to the expected data
    debug("starting comparison");
    for (size_t i = 0; i < len; ++i) {

        verbose("Comparing byte %u", i);

        if (read_data[i] != (uint8_t)expected_data[i]) {

            snprintf(result, result_len, "ERR Mismatch at byte %zu: expected 0x%02X, got 0x%02X", i, (uint8_t)expected_data[i], read_data[i]);
            warning("Mismatch at byte %zu: expected 0x%02X, got 0x%02X", i, (uint8_t)expected_data[i], read_data[i]);
            returnVal = false;
            goto end;
        }
    }

    // Woohoo! Everything matches
    info("EEPROM data verified successfully!");
    snprintf(result, result_len, "OK Data verified successfully!");
    returnVal = true;
    goto end;


    end:
    // Release the buffer
    if(read_data != NULL) {
        vPortFree(read_data);
    }
    return returnVal;
}


#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <FreeRTOS.h>
#include <task.h>

#include <hardware/gpio.h>
#include <hardware/watchdog.h>

// TinyUSB
#include "tusb.h"

#include "config.h"
#include "i2c_programmer.h"
#include "logging/logging.h"
#include "shell.h"
#include "types.h"
#include "usb.h"
#include "version.h"
#include "watchdog/watchdog.h"

TaskHandle_t shell_task_handle = NULL;

extern volatile size_t xFreeHeapSpace;

extern u8* incoming_data_buffer;
extern u32 incomingBufferIndex;

extern u8* request_buffer;
extern u32 requestBufferIndex;

extern enum ProgrammerState programmer_state;
extern u32 program_size;




void handle_shell_command(u8 *buffer) {

    debug("handling command: %s", buffer);

    u8 command = buffer[0];

    switch (command) {

        case 'B':
            info("burning the EEPROM...");

            if(program_size == 0) {
                warning("No data to burn!");
                send_response("ERR No data to burn!");
                reset_request_buffer();
                break;
            }

            write_incoming_buffer_to_eeprom();
            send_response("OK");
            reset_request_buffer();
            break;

        case 'V':
            info("verifying the EEPROM...");

            if(program_size == 0) {
                warning("No data to verify!");
                send_response("ERR No data to verify!");
                reset_request_buffer();
                break;
            }

            char result[100];

            verify_eeprom_data(PROGRAMMER_I2C_BUS, PROGRAMMER_I2C_ADDR, incoming_data_buffer, program_size,
                                result, sizeof(result));
            reset_request_buffer();
            send_response(result);
            break;

        case 'H':
            info("help command");
            const char *help_text0 = "\nI - Info in machine-readable format\nB - Burn\nV - Verify\nP - Print incoming data buffer";
            const char *help_text1 = "R - Reboot\nL##### - Load # bytes\n<ESC> - Reset Request Buffer\nH - Help";
            send_response(help_text0);
            send_response(help_text1);

            char help_buffer[OUTGOING_RESPONSE_BUFFER_SIZE];

            memset(help_buffer, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
            snprintf(help_buffer, OUTGOING_RESPONSE_BUFFER_SIZE, "\nFree heap: %u bytes\n", xFreeHeapSpace);
            send_response(help_buffer);

            memset(help_buffer, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
            snprintf(help_buffer, OUTGOING_RESPONSE_BUFFER_SIZE, "This is version %s.\n", CREATURE_FIRMWARE_VERSION_STRING);
            send_response(help_buffer);

            reset_request_buffer();
            break;

        case 'I':
            info("info command");

            char info_buffer[OUTGOING_RESPONSE_BUFFER_SIZE];

            memset(info_buffer, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
            snprintf(info_buffer, OUTGOING_RESPONSE_BUFFER_SIZE, "{\"version\": \"%s\", \"free_heap\": %u, \"uptime\": %lu}",
                     CREATURE_FIRMWARE_VERSION_STRING,
                     xFreeHeapSpace,
                     to_ms_since_boot(get_absolute_time()));
            send_response(info_buffer);

            reset_request_buffer();
            break;

        case 'P':
            info("printing incoming data buffer");
            print_incoming_data_buffer();
            reset_request_buffer();
            break;

        case 'R':
            info("rebooting...");
            send_response("BYE!");
            vTaskDelay(pdMS_TO_TICKS(30)); // Give the response time to send
            reboot();
            break;

        case 'L':
            info("loading data...");

            // Move the buffer pointer to the start of the data
            char *ending_pointer;
            u8 *data = buffer + 1;
            long size = strtol((char*)data, &ending_pointer, 10);

            // Check for any errors during conversion
            if ((errno == ERANGE && (size == LONG_MAX || size == LONG_MIN)) || size > UINT32_MAX) {
                info("Conversion error occurred: out of range");
                send_response("ERR Size is out of range");
                reset_request_buffer();
                break;
            }

            if (*ending_pointer != '\0') {
                warning("Conversion error: non-numeric characters found: %s\n", ending_pointer);
                send_response("ERR Size is non-numeric");
                reset_request_buffer();
                break;
            }

            program_size = (u32)size;
            debug("program size: %u", program_size);

            if(program_size == 0) {
                warning("Conversion error: size is zero");
                send_response("ERR Size is zero");
                reset_request_buffer();
                break;
            }

            if(program_size > INCOMING_BUFFER_SIZE) {
                warning("Conversion error: size is too large");

                char response[OUTGOING_RESPONSE_BUFFER_SIZE];
                memset(response, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
                snprintf(response, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Size too large: %lu (max is %lu)",
                         program_size,
                         (u32)INCOMING_BUFFER_SIZE);

                send_response(response);
                reset_request_buffer();
                break;
            }

            // Reset the buffer and switch to the FILLING_BUFFER state
            reset_incoming_buffer();
            programmer_state = FILLING_BUFFER;
            send_response("GO_AHEAD");
            reset_request_buffer();
            break;

//        case 'D':
//            info("loading data...");
//
//            // Move the buffer pointer to the start of the data
//            char *ending_pointer;
//            u8 *data = buffer + 1;
//            long size = strtol((char*)data, &ending_pointer, 10);
//
//            // Check for any errors during conversion
//            if ((errno == ERANGE && (size == LONG_MAX || size == LONG_MIN)) || size > UINT32_MAX) {
//                info("Conversion error occurred: out of range");
//                send_response("ERR Size is out of range");
//                reset_request_buffer();
//                break;
//            }
//
//            if (*ending_pointer != '\0') {
//                warning("Conversion error: non-numeric characters found: %s\n", ending_pointer);
//                send_response("ERR Size is non-numeric");
//                reset_request_buffer();
//                break;
//            }
//
//            program_size = (u32)size;
//            debug("program size: %u", program_size);
//
//            if(program_size == 0) {
//                warning("Conversion error: size is zero");
//                send_response("ERR Size is zero");
//                reset_request_buffer();
//                break;
//            }
//
//            if(program_size > INCOMING_BUFFER_SIZE) {
//                warning("Conversion error: size is too large");
//
//                char response[OUTGOING_RESPONSE_BUFFER_SIZE];
//                memset(response, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
//                snprintf(response, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Size too large: %lu (max is %lu)",
//                         program_size,
//                         (u32)INCOMING_BUFFER_SIZE);
//
//                send_response(response);
//                reset_request_buffer();
//                break;
//            }
//
//            // Reset the buffer and switch to the FILLING_BUFFER state
//            reset_incoming_buffer();
//            programmer_state = FILLING_BUFFER;
//            send_response("GO_AHEAD");
//            reset_request_buffer();
//            break;

        default:
            warning("unknown command: %s", buffer);

            char response[OUTGOING_RESPONSE_BUFFER_SIZE];
            memset(response, '\0', OUTGOING_RESPONSE_BUFFER_SIZE);
            snprintf(response, OUTGOING_RESPONSE_BUFFER_SIZE, "ERR Unknown command: %s (use H for help)", buffer);

            send_response(response);
            reset_request_buffer();
            break;
    }
}


void terminate_shell() {

    // Clean up the buffers
    reset_request_buffer();
    reset_incoming_buffer();

    if (shell_task_handle != NULL) {
        vTaskDelete(shell_task_handle);
        shell_task_handle = NULL;
        info("shell task terminated");
    } else {
        warning("shell task already terminated");
    }
}

void launch_shell() {

    if (shell_task_handle != NULL) {
        warning("shell task already running");
        return;
    }

    // Clean up the buffers
    reset_request_buffer();
    reset_incoming_buffer();

    xTaskCreate(shell_task,
                "shell_task",
                configMINIMAL_STACK_SIZE + 256,
                NULL,
                1,
                &shell_task_handle);
    info("shell task launched");
}

void reset_incoming_buffer() {
    debug("resetting incoming buffer");
    memset(incoming_data_buffer, '\0', INCOMING_BUFFER_SIZE);
    incomingBufferIndex = 0;
}

void reset_request_buffer() {
    debug("resetting request buffer");
    memset(request_buffer, '\0', INCOMING_REQUEST_BUFFER_SIZE);
    requestBufferIndex = 0;
}

void send_response(const char *response) {

    // Make a buffer to hold the response
    u32 incoming_length = strlen(response);

    u32 message_length = incoming_length < OUTGOING_RESPONSE_BUFFER_SIZE ? incoming_length : OUTGOING_RESPONSE_BUFFER_SIZE;

    u32 buffer_size = message_length + 2; // Add 2 for newline and null-terminator
    char *response_buffer = pvPortMalloc(buffer_size);

    // Make sure we got a buffer
    if (response_buffer == NULL) {
        warning("unable to allocate a buffer for the response!");
        goto exit_point;
    }

    // Blank out the buffer
    memset(response_buffer, '\0', buffer_size);

    // Copy the response into the buffer
    stpncpy(response_buffer, response, message_length);
    response_buffer[message_length] = '\n'; // Add a newline

    // And send it
    cdc_send(response_buffer);
    goto exit_point;



    exit_point:
    if(response_buffer != NULL)
        vPortFree(response_buffer);
}

portTASK_FUNCTION(shell_task, pvParameters) {

    // Reset the buffers
    reset_incoming_buffer();
    reset_request_buffer();

    // Not a lot to do right now
    for (EVER) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        debug("shell active. incomingBufferIndex: %lu, requestBufferIndex %lu",
              incomingBufferIndex, requestBufferIndex);
    }

}


// Automatically called by TinyUSB when data is received
void tud_cdc_rx_cb(__attribute__((unused)) uint8_t itf) {
    // Turn on the receiving LED
    gpio_put(INCOMING_LED_PIN, true);

    // Get the number of available bytes
    u32 count = tud_cdc_available();
    if (count == 0) return;  // Nothing to chew on yet!

    // Temporary buffer to hold incoming data
    u8 tempBuffer[count];
    u32 readCount = tud_cdc_read(tempBuffer, count);

    for (u32 i = 0; i < readCount; i++) {
        char ch = tempBuffer[i];
        debug("Received character: %c", ch);

#if LOGGING_LEVEL > 4
        // Log alphanumerics normally, others in hex
        if (isalnum((unsigned char)ch)) {
            verbose("Received character: %c", ch);
        } else {
            char hexStr[5]; // Enough for "0xHH" and a null terminator
            snprintf(hexStr, sizeof(hexStr), "0x%02X", (unsigned char)ch);
            verbose("Received character: %s", hexStr);
        }
#endif

        // Process data based on the current state of our programmer
        switch (programmer_state) {
            case FILLING_BUFFER:
                // Reserve one byte for null termination
                if (incomingBufferIndex < INCOMING_BUFFER_SIZE - 1) {
                    incoming_data_buffer[incomingBufferIndex++] = ch;
                    debug("Incoming buffer index: %u", incomingBufferIndex);

                    // Check if we've received the expected number of bytes
                    if (incomingBufferIndex == program_size) {
                        // Null-terminate right after the last valid byte
                        incoming_data_buffer[incomingBufferIndex] = '\0';
                        programmer_state = IDLE;
                        info("All data received; switching to IDLE state");
                        send_response("OK");
                    }
                } else {
                    warning("Buffer overflow in incoming data buffer");
                    programmer_state = ERROR;
                }
                break;

            case IDLE:
                if (ch == 0x1B) {  // Escape: reset buffers (a clean slate, like fresh lettuce!)
                    debug("Received ESC - resetting buffers");
                    reset_request_buffer();
                    reset_incoming_buffer();
                    break;
                }
                else if (ch == 0x0A || ch == 0x0D) {  // Newline: end of command
                    if (requestBufferIndex == 0) {
                        warning("Skipping blank input line from sender");
                        break;
                    }
                    // Null-terminate the request and process it
                    request_buffer[requestBufferIndex] = '\0';
                    handle_shell_command(request_buffer);
                    // Reset the request buffer after processing the command
                    reset_request_buffer();
                }
                else {
                    // Append to the request buffer if there's room
                    if (requestBufferIndex < INCOMING_REQUEST_BUFFER_SIZE - 1) {
                        request_buffer[requestBufferIndex++] = ch;
                        debug("Request buffer index: %u", requestBufferIndex);
                    } else {
                        warning("Request buffer overflow on incoming request");
                        // Reset the request buffer to start fresh on next command
                        reset_request_buffer();
                    }
                }
                break;

            default:
                warning("Received data in an unexpected state: %d", programmer_state);
                break;
        }
    }

    // Final safety check: null-terminate the incoming data buffer at the current index
    if (incomingBufferIndex < INCOMING_BUFFER_SIZE) {
        incoming_data_buffer[incomingBufferIndex] = '\0';
    }
}



void print_incoming_data_buffer() {

    // Make sure we're not trying to print nothing
    if(incoming_data_buffer == NULL || incomingBufferIndex == 0) {
        warning("No data to print");
        return;
    }

    // Buffer to hold a line of output
    char line_buffer[140];

    send_response("--- Start of Incoming Data Buffer ---");

    for (size_t i = 0; i < incomingBufferIndex; i++) {
        if (i % 32 == 0) {
            // Start a new line with the memory address, clear the buffer first
            memset(line_buffer, 0, sizeof(line_buffer));
            snprintf(line_buffer, sizeof(line_buffer), "  0x%04zx: ", i);
        }

        // Append the next byte to the current line, checking buffer space
        snprintf(line_buffer + strlen(line_buffer), sizeof(line_buffer) - strlen(line_buffer), "%02X ", incoming_data_buffer[i]);


        // If 32 bytes have been added, or it's the end of the data, print the line
        if (i % 32 == 31 || i == incomingBufferIndex - 1) {
            send_response(line_buffer);

            // Give the debugger's UART a chance to catch up
            vTaskDelay(pdMS_TO_TICKS(15));
        }
    }

    send_response("--- End of Incoming Data Buffer ---");
}

#include <stddef.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/binary_info.h"
#include "pico/stdlib.h"

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"

#include "debug/blinker.h"
#include "logging/logging.h"
#include "logging/logging_api.h"
#include "programmer/i2c_programmer.h"

#include "config.h"
#include "shell.h"
#include "types.h"
#include "usb.h"
#include "version.h"
#include "watchdog/watchdog.h"

volatile size_t xFreeHeapSpace;


// Large buffer to store incoming data
u8* incoming_data_buffer;
u32 incomingBufferIndex = 0;

u8* request_buffer;
u32 requestBufferIndex = 0;

// What state are we in?
enum ProgrammerState programmer_state = IDLE;
u32 program_size = 0;

portTASK_FUNCTION_PROTO(startup_task, pvParameters);

/** Print the message to the console */
void acw_post_logging_hook(char *message, uint8_t message_length) {

    (void) message_length;

    // Send the message to the console
    printf("%s\n", message);

}

void allocate_buffers();


int main() {

    bi_decl(bi_program_name("programmer"))
    bi_decl(bi_program_description("April's Creature Workshop i2c EEPROM Programmer"))
    bi_decl(bi_program_version_string(CREATURE_FIRMWARE_VERSION_STRING))
    bi_decl(bi_program_feature("FreeRTOS Version: " tskKERNEL_VERSION_NUMBER))
    bi_decl(bi_2pins_with_func(PROGRAMMER_SDA_PIN, PROGRAMMER_SCL_PIN, GPIO_FUNC_I2C))
    bi_decl(bi_1pin_with_name(CDC_MOUNTED_LED_PIN, "CDC Mounted LED"))
    bi_decl(bi_1pin_with_name(INCOMING_LED_PIN, "Data Received LED"))
    bi_decl(bi_1pin_with_name(OUTGOING_LED_PIN, "Data Transmitted LED"))


    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");

    if (watchdog_caused_reboot()) {
        warning("*** Last reset was caused by the watchdog timer! ***");
    } else {
        debug("clean boot");
    }

    // Set up the board
    board_init();

    // Allocate the giant buffer for the incoming data
    allocate_buffers();

    programmer_setup_i2c();

    // Blinky the onboard LED so we know the cores are running
    start_debug_blinker();


    // Queue up the startup task for right after the scheduler starts
    TaskHandle_t startup_task_handle;
    xTaskCreate(startup_task,
                "startup_task",
                configMINIMAL_STACK_SIZE,
                NULL,
                1,
                &startup_task_handle);

    // Start the watchdog timer so we reboot if we hang
    start_watchdog_timer();

    vTaskStartScheduler();
}

portTASK_FUNCTION(startup_task, pvParameters) {
    usb_init();
    usb_start();
    vTaskDelete(NULL);
}

void allocate_buffers() {

    // Incoming Data Buffer
    incomingBufferIndex = 0;
    incoming_data_buffer = pvPortMalloc(INCOMING_BUFFER_SIZE);
    if (incoming_data_buffer == NULL) {
        configASSERT("unable to allocate a buffer for incoming messages!");
        return;
    }
    memset(incoming_data_buffer, '\0', INCOMING_BUFFER_SIZE);
    info("incoming data buffer allocated");


    // Incoming Request Buffer
    request_buffer = pvPortMalloc(INCOMING_REQUEST_BUFFER_SIZE);
    if (request_buffer == NULL) {
        configASSERT("unable to allocate a buffer for incoming requests!");
        return;
    }

}
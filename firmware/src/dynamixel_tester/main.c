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
#include "dynamixel/dynamixel_hal.h"
#include "logging/logging.h"
#include "logging/logging_api.h"

#include "config.h"
#include "shell.h"
#include "types.h"
#include "usb.h"
#include "version.h"
#include "watchdog/watchdog.h"

volatile size_t xFreeHeapSpace;

u8 *request_buffer;
u32 requestBufferIndex = 0;

dxl_hal_context_t *dxl_ctx = NULL;

portTASK_FUNCTION_PROTO(startup_task, pvParameters);

/** Print the message to the console */
void acw_post_logging_hook(char *message, uint8_t message_length) {
    (void) message_length;
    printf("%s\n", message);
}

void allocate_buffers();

int main() {

    bi_decl(bi_program_name("dynamixel-tester"))
    bi_decl(bi_program_description("April's Creature Workshop Dynamixel Servo Tester"))
    bi_decl(bi_program_version_string(CREATURE_FIRMWARE_VERSION_STRING))
    bi_decl(bi_program_feature("FreeRTOS Version: " tskKERNEL_VERSION_NUMBER))
    bi_decl(bi_1pin_with_name(DXL_DATA_PIN, "Dynamixel Data"))
    bi_decl(bi_1pin_with_name(CDC_MOUNTED_LED_PIN, "CDC Mounted LED"))
    bi_decl(bi_1pin_with_name(INCOMING_LED_PIN, "Data Received LED"))
    bi_decl(bi_1pin_with_name(OUTGOING_LED_PIN, "Data Transmitted LED"))

    stdio_init_all();

    logger_init();
    debug("Logging running!");

    if (watchdog_caused_reboot()) {
        warning("*** Last reset was caused by the watchdog timer! ***");
    } else {
        debug("clean boot");
    }

    board_init();

    allocate_buffers();

    // Initialize the Dynamixel HAL
    dxl_hal_config_t dxl_config = {
        .data_pin = DXL_DATA_PIN,
        .baud_rate = DXL_DEFAULT_BAUD_RATE,
        .pio = DXL_PIO_INSTANCE,
    };
    dxl_ctx = dxl_hal_init(&dxl_config);
    if (dxl_ctx == NULL) {
        fatal("Failed to initialize Dynamixel HAL!");
    }

    start_debug_blinker();

    TaskHandle_t startup_task_handle;
    xTaskCreate(startup_task,
                "startup_task",
                configMINIMAL_STACK_SIZE,
                NULL,
                1,
                &startup_task_handle);

    vTaskStartScheduler();
}

portTASK_FUNCTION(startup_task, pvParameters) {
    usb_init();
    usb_start();
    vTaskDelete(NULL);
}

void allocate_buffers() {
    request_buffer = pvPortMalloc(INCOMING_REQUEST_BUFFER_SIZE);
    if (request_buffer == NULL) {
        configASSERT("unable to allocate a buffer for incoming requests!");
        return;
    }
    memset(request_buffer, '\0', INCOMING_REQUEST_BUFFER_SIZE);
    info("request buffer allocated");
}

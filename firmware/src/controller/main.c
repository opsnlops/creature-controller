

#include <stddef.h>
#include <stdlib.h>

#include "config.h"

#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"

#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "controller/controller.h"
#include "device/power_relay.h"
#include "device/status_lights.h"
#include "debug/sensor_reporter.h"
#include "debug/stats_reporter.h"
#include "io/i2c.h"
#include "io/message_processor.h"
#include "io/spi.h"
#include "io/usb_serial.h"
#include "io/uart_serial.h"
#include "logging/logging.h"
#include "sensor/sensor.h"
#include "usb/usb.h"
#include "watchdog/watchdog.h"

#include "types.h"
#include "version.h"


volatile size_t xFreeHeapSpace;

portTASK_FUNCTION_PROTO(startup_task, pvParameters);

int main() {

    bi_decl(bi_program_name("controller-firmware"))
    bi_decl(bi_program_description("April's Creature Workshop Controller"))
    bi_decl(bi_program_version_string(CREATURE_FIRMWARE_VERSION_STRING))
    bi_decl(bi_program_feature("FreeRTOS Version: " tskKERNEL_VERSION_NUMBER))
    bi_decl(bi_program_feature(CREATURE_PROTOCOL_VERSION_STRING))
    bi_decl(bi_program_feature("Baud: 115200,N,8,1"))
    bi_decl(bi_program_url("https://creature.engineering/hardware/creature-controller/"))
    bi_decl(bi_1pin_with_name(POWER_PIN, "Power Relay"))
    bi_decl(bi_1pin_with_name(STATUS_LIGHTS_LOGIC_BOARD_PIN, "Status Lights for Logic Board"))
    bi_decl(bi_1pin_with_name(STATUS_LIGHTS_MOD_A_PIN, "Status Lights Module A"))
    bi_decl(bi_1pin_with_name(STATUS_LIGHTS_MOD_B_PIN, "Status Lights Module B"))
    bi_decl(bi_1pin_with_name(STATUS_LIGHTS_MOD_C_PIN, "Status Lights Module C"))
    bi_decl(bi_2pins_with_func(UART_TX_PIN, UART_RX_PIN, GPIO_FUNC_UART))
    bi_decl(bi_2pins_with_func(SENSORS_I2C_SDA_PIN, SENSORS_I2C_SCL_PIN, GPIO_FUNC_I2C))
    bi_decl(bi_4pins_with_func(SENSORS_SPI_SCK_PIN, SENSORS_SPI_TX_PIN, SENSORS_SPI_RX_PIN, SENSORS_SPI_CS_PIN, GPIO_FUNC_SPI))
    bi_decl(bi_1pin_with_name(SERVO_0_GPIO_PIN, "Servo 0"))
    bi_decl(bi_1pin_with_name(SERVO_1_GPIO_PIN, "Servo 1"))
    bi_decl(bi_1pin_with_name(SERVO_2_GPIO_PIN, "Servo 2"))
    bi_decl(bi_1pin_with_name(SERVO_3_GPIO_PIN, "Servo 3"))
    bi_decl(bi_1pin_with_name(SERVO_4_GPIO_PIN, "Servo 4"))
    bi_decl(bi_1pin_with_name(SERVO_5_GPIO_PIN, "Servo 5"))
    bi_decl(bi_1pin_with_name(SERVO_6_GPIO_PIN, "Servo 6"))
    bi_decl(bi_1pin_with_name(SERVO_7_GPIO_PIN, "Servo 7"))
    bi_decl(bi_1pin_with_name(USB_MOUNTED_LED_PIN, "USB Mounted LED"))
    bi_decl(bi_1pin_with_name(CONTROLLER_RESET_PIN, "Controller Reset"))

    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");

    // Log loudly if we were reset by the watchdog
    if (watchdog_caused_reboot()) {
        warning("*** Last reset was caused by the watchdog timer! ***");
    } else {
        debug("clean boot, not triggered by watchdog");
    }

    // Set up the board
    board_init();

    // Set up the power relay
    init_power_relay();

    // Bring up the message processor
    message_processor_init();
    usb_serial_init();
    uart_serial_init();

    // Start the I/O bits
    message_processor_start();
    usb_serial_start();
    uart_serial_start();

    // Start the controller
    controller_init();
    controller_start();

    // Fire up the stats reporter
    start_stats_reporter();

    // Turn on the status lights
    status_lights_init();
    status_lights_start();

#if USE_SENSORS
    // Configure i2c for our needs
    setup_i2c();

    // Set up spi
    setup_spi();

    // Start monitoring our sensors
    sensor_init();
    sensor_start();

    // Fire up the sensor reporter
    start_sensor_reporter();
#else
    // Mark the build as not having sensor enabled
    warning("   *** NOTE: Sensors are disabled in this build! ***");
    bi_decl(bi_program_feature(" ->> *** NOTE: Sensors have been disabled in this build *** <<-"))
#endif

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

    // And fire up the tasks!
    vTaskStartScheduler();

}


portTASK_FUNCTION(startup_task, pvParameters) {

    // These are in a task because the docs say:

    /*
        init device stack on configured roothub port
        This should be called after scheduler/kernel is started.
        Otherwise, it could cause kernel issue since USB IRQ handler does use RTOS queue API.
     */

    usb_init();
    usb_start();

    // Bye!
    vTaskDelete(NULL);

}
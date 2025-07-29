/**
 * @file main.c
 * @brief Main entry point for the April's Creature Workshop Controller firmware
 *
 * This file provides the central initialization sequence and system startup
 * for the creature controller firmware. It orchestrates the initialization
 * of all subsystems in the correct order and starts the RTOS scheduler.
 */

#include <stddef.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include "bsp/board.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "config.h"
#include "controller/controller.h"
#include "debug/sensor_reporter.h"
#include "debug/stats_reporter.h"
#include "device/eeprom.h"
#include "device/power_control.h"
#include "device/status_lights.h"
#include "io/i2c.h"
#include "io/message_processor.h"
#include "io/spi.h"
#include "io/uart_serial.h"
#include "io/usb_serial.h"
#include "logging/logging.h"
#include "sensor/sensor.h"
#include "types.h"
#include "usb/usb.h"
#include "usb/usb_descriptors.h"
#include "version.h"
#include "watchdog/watchdog.h"

// Forward declarations for initialization functions
static void initialize_binary_info(void);
static void initialize_eeprom(void);
static bool initialize_core_systems(void);
static bool initialize_communication_systems(void);
static bool initialize_controller_systems(void);
static bool initialize_status_and_monitoring(void);
static bool schedule_startup_task(void);
static void log_system_info(void);

// Startup task prototype
portTASK_FUNCTION_PROTO(startup_task, pvParameters);

// Heap space tracking
volatile size_t xFreeHeapSpace;

/**
 * @brief Main entry point for the program
 *
 * Initializes all subsystems in a structured sequence and starts the RTOS scheduler.
 * The initialization follows this sequence:
 * 1. Binary info for debugging
 * 2. Core systems (logging, stdlib)
 * 3. EEPROM configuration (if enabled)
 * 4. Communication systems
 * 5. Controller systems
 * 6. Status and monitoring
 * 7. Watchdog timer
 * 8. RTOS scheduler
 *
 * @return -1 on failure, though successful execution does not return
 */
int main() {
    // Setup binary info first for debugging
    initialize_binary_info();

    // Initialize core systems (logging, stdlib)
    if (!initialize_core_systems()) {
        // Can't log since logging may have failed
        return -1;
    }

    // Log system information
    log_system_info();

    // Read the EEPROM before setting up the USB subsystem
    initialize_eeprom();

    // Initialize communication systems
    if (!initialize_communication_systems()) {
        error("Failed to initialize communication systems");
        return -1;
    }

    // Initialize controller systems
    if (!initialize_controller_systems()) {
        error("Failed to initialize controller systems");
        return -1;
    }

    // Initialize status tracking and monitoring
    if (!initialize_status_and_monitoring()) {
        error("Failed to initialize monitoring systems");
        // Non-fatal, continue anyway
    }

    // Schedule startup task for post-scheduler tasks
    if (!schedule_startup_task()) {
        error("Failed to schedule startup task");
        return -1;
    }

    // Initialize the watchdog timer
    if (!init_watchdog()) {
        warning("Failed to initialize watchdog timer - continuing without watchdog protection");
    } else {
        debug("Watchdog init'ed successfully");
    }

    info("All systems initialized, starting scheduler");

    // Start the FreeRTOS scheduler - this should never return
    vTaskStartScheduler();

    // If we get here, something went very wrong
    fatal("Scheduler failed to start!");
    return -1;
}

/**
 * @brief Create binary info declarations for debugging
 *
 * Sets up Pico binary info entries that can be read via picotool.
 * This includes firmware version, GPIO pin assignments, and other
 * critical hardware configuration details.
 */
static void initialize_binary_info(void) {
    bi_decl(bi_program_name("controller-firmware"))
    bi_decl(bi_program_description("April's Creature Workshop Controller"))
    bi_decl(bi_program_version_string(CREATURE_FIRMWARE_VERSION_STRING))
    bi_decl(bi_program_feature("FreeRTOS Version: " tskKERNEL_VERSION_NUMBER))
    bi_decl(bi_program_feature(CREATURE_PROTOCOL_VERSION_STRING))
    bi_decl(bi_program_feature("Baud: 115200,N,8,1"))
    bi_decl(bi_program_url("https://creature.engineering/hardware/creature-controller/"))
    bi_decl(bi_1pin_with_name(POWER_PIN, "Power Relay"))
    bi_decl(bi_1pin_with_name(STATUS_LIGHTS_LOGIC_BOARD_PIN, "Status Lights for Logic Board"))
    bi_decl(bi_1pin_with_name(STATUS_LIGHTS_SERVOS_PIN, "Status Lights for the Servos"))
    bi_decl(bi_2pins_with_func(UART_TX_PIN, UART_RX_PIN, GPIO_FUNC_UART))
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

#ifdef CC_VER2
    bi_decl(bi_program_feature("Requires Hardware Version: 2"))
#endif
#ifdef CC_VER3
    bi_decl(bi_program_feature("Requires Hardware Version: 3"))
    bi_decl(bi_1pin_with_name(SERVO_0_POWER_PIN, "Servo 0 Power Control"))
    bi_decl(bi_1pin_with_name(SERVO_1_POWER_PIN, "Servo 1 Power Control"))
    bi_decl(bi_1pin_with_name(SERVO_2_POWER_PIN, "Servo 2 Power Control"))
    bi_decl(bi_1pin_with_name(SERVO_3_POWER_PIN, "Servo 3 Power Control"))
    bi_decl(bi_1pin_with_name(SERVO_4_POWER_PIN, "Servo 4 Power Control"))
    bi_decl(bi_1pin_with_name(SERVO_5_POWER_PIN, "Servo 5 Power Control"))
    bi_decl(bi_1pin_with_name(SERVO_6_POWER_PIN, "Servo 6 Power Control"))
    bi_decl(bi_1pin_with_name(SERVO_7_POWER_PIN, "Servo 7 Power Control"))
#endif
}

/**
 * @brief Initialize the EEPROM and configure system from stored settings
 *
 * If EEPROM support is enabled, this function initializes the I2C interface
 * for the EEPROM, reads the stored configuration, and updates system settings
 * including USB descriptors. If disabled, it logs a warning.
 */
void initialize_eeprom(void) {
#if USE_EEPROM
    bi_decl(bi_2pins_with_func(EEPROM_SDA_PIN, EEPROM_SCL_PIN, GPIO_FUNC_I2C))
    eeprom_setup_i2c();
    read_eeprom_and_configure();
    usb_descriptors_init();
#else
    // Mark the build as not having EEPROM enabled
    warning("   *** NOTE: EEPROM is disabled in this build! ***");
    bi_decl(bi_program_feature(" ->> *** NOTE: EEPROM has been disabled in this build *** <<-"))
#endif
}

/**
 * @brief Initialize core system components
 *
 * Sets up the fundamental systems needed for basic operation:
 * - Standard I/O for debugging
 * - Logging system
 * - Board hardware initialization
 *
 * @return true if successfully initialized, false otherwise
 */
static bool initialize_core_systems(void) {
    // Initialize stdio
    stdio_init_all();

    // Initialize logging system
    logger_init();
    debug("Logging system initialized");

    // Initialize board hardware
    board_init();
    debug("Board hardware initialized");

    debug("Core systems initialized");
    return true;
}

/**
 * @brief Log system version and boot information
 *
 * Outputs key system information to the log including firmware version,
 * FreeRTOS version, and protocol version. Also logs warnings if certain
 * subsystems are disabled in the build.
 */
static void log_system_info(void) {
    info("----------------------------------------");
    info("April's Creature Workshop Controller v%s", CREATURE_FIRMWARE_VERSION_STRING);
    info("FreeRTOS Version: %s", tskKERNEL_VERSION_NUMBER);
    info(CREATURE_PROTOCOL_VERSION_STRING);
    info("Platform: %s", PICO_PLATFORM_STRING);

#if USE_SENSORS == 0
    warning("*** Sensors are disabled in this build! ***");
#endif

#if USE_EEPROM == 0
    warning("*** Configuration via the EEPROM is disabled in this build! ***");
#endif

    // Log watchdog reset status
    if (watchdog_caused_reboot()) {
        warning("*** Last reset was caused by watchdog timer! ***");
    } else {
        debug("Clean boot, not triggered by watchdog");
    }
    info("----------------------------------------");
}

/**
 * @brief Initialize communication subsystems
 *
 * Sets up all communication interfaces and starts their respective tasks:
 * - Message processor
 * - USB serial
 * - UART serial
 *
 * @return true if successfully initialized, false otherwise
 */
static bool initialize_communication_systems(void) {
    // Bring up the message processor
    message_processor_init();
    debug("Message processor initialized");

    // Initialize USB serial communication
    usb_serial_init();
    debug("USB serial initialized");

#ifdef CC_VER2
    // Initialize UART serial communication
    uart_serial_init();
    debug("UART serial initialized");
#endif

    // Start the communication systems
    message_processor_start();
    usb_serial_start();

#ifdef CC_VER2
    uart_serial_start();
#endif

    debug("Communication systems initialized and started");
    return true;
}

/**
 * @brief Initialize controller and related hardware systems
 *
 * Sets up the core control systems:
 * - Power relay for motor control
 * - Main controller (servo/motor control)
 * - Status lights for visual feedback
 *
 * @return true if successfully initialized, false otherwise
 */
static bool initialize_controller_systems(void) {

#ifdef CC_VER3
    // Initialize the power control pins for all motors
    init_motor_power_control();
    debug("set up the power control pins for all motors");
#endif

    // Initialize the controller
    controller_init();
    controller_start();
    debug("Controller initialized and started");

    // Initialize status lights
    status_lights_init();
    status_lights_start();
    debug("Status lights initialized and started");

    debug("Controller systems initialized");
    return true;
}

/**
 * @brief Initialize status reporting and monitoring systems
 *
 * Sets up systems for monitoring and reporting on device state:
 * - Statistics reporter
 * - I2C and SPI buses for sensors (if enabled)
 * - Sensor monitoring and reporting
 *
 * @return true if successfully initialized, false otherwise
 */
static bool initialize_status_and_monitoring(void) {
    // Fire up the stats reporter
    start_stats_reporter();
    debug("Stats reporter started");

#if USE_SENSORS
    bi_decl(bi_2pins_with_func(SENSORS_I2C_SDA_PIN, SENSORS_I2C_SCL_PIN, GPIO_FUNC_I2C))
#if CC_VER2
    bi_decl(bi_4pins_with_func(SENSORS_SPI_SCK_PIN, SENSORS_SPI_TX_PIN, SENSORS_SPI_RX_PIN, SENSORS_SPI_CS_PIN, GPIO_FUNC_SPI))
#endif
    // Configure i2c for our needs
    if (!setup_i2c()) {
        error("Failed to initialize I2C");
        return false;
    }

#ifdef CC_VER2
    // Set up spi
    if (!setup_spi()) {
        error("Failed to initialize SPI");
        return false;
    }
    debug("I2C and SPI initialized");
#endif


    // Start monitoring our sensors
    sensor_init();
    sensor_start();
    debug("Sensors initialized and started");

    // Fire up the sensor reporter
    start_sensor_reporter();
    debug("Sensor reporter started");
#else
    // Mark the build as not having sensor enabled
    warning("   *** NOTE: Sensors are disabled in this build! ***");
    bi_decl(bi_program_feature(" ->> *** NOTE: Sensors have been disabled in this build *** <<-"))
#endif

    debug("Status and monitoring systems initialized");
    return true;
}

/**
 * @brief Schedule the startup task
 *
 * Creates a task that will run after the RTOS scheduler starts to handle
 * initialization steps that must occur after the scheduler is running.
 *
 * @return true if successfully scheduled, false otherwise
 */
static bool schedule_startup_task(void) {
    TaskHandle_t startup_task_handle = NULL;

    const BaseType_t task_create_result = xTaskCreate(
            startup_task,
            "startup_task",
            configMINIMAL_STACK_SIZE,
            NULL,
            1,
            &startup_task_handle
    );

    if (task_create_result != pdPASS || startup_task_handle == NULL) {
        error("Failed to create startup task");
        return false;
    }

    debug("Startup task scheduled");
    return true;
}

/**
 * @brief Task to handle initialization after scheduler has started
 *
 * This task initializes the USB subsystem, which must occur after the RTOS
 * scheduler is running because the USB driver uses RTOS functionality.
 * The task deletes itself once initialization is complete.
 *
 * @param pvParameters Task parameters (unused)
 */
portTASK_FUNCTION(startup_task, pvParameters) {

    (void)pvParameters;

    // Start off with all of the motors off, in case we are recovering from
    // a weird state
    disable_all_motors();

    // Initialize USB after scheduler is started (required by TinyUSB)
    usb_init();
    usb_start();
    debug("USB initialized and started");

    // Task complete - delete self
    vTaskDelete(NULL);
}
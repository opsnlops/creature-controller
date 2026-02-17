#include <limits.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <timers.h>

// Our modules
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/watchdog.h"

#include "device/power_control.h"
#include "io/message_processor.h"
#include "io/responsive_analog_read_filter.h"
#include "logging/logging.h"

#include "config.h"
#include "controller.h"
#include "types.h"
#include "version.h"

#ifdef CC_VER4
#include "dynamixel/dynamixel_hal.h"
#include "dynamixel/dynamixel_servo.h"
#include <stdio.h>
#include <task.h>
#endif

// Stats
extern volatile u64 number_of_pwm_wraps;

// Counter of how many times we've the PWM counter roll over since the last
// watchdog update
volatile u32 watchdog_wrap_count = 0UL;

// Mutex for thread-safe access to motor_map
SemaphoreHandle_t motor_map_mutex;

/*
 * The following map is used to map motor IDs to GPIO pins!
 *
 * The bit shifts come from the Pico SDK. In order to make this map be created
 * at build time they needed to be a constant value, so I copied them here.
 *
 * See `pwm.h` in the Pico SDK for more information! 😅
 *
 * Note: Motor IDs 0-7 now map to descending GPIO pins (13 down to 6)
 * Each motor also has an associated power control pin.
 * All motors start unconfigured until the computer sends configuration data.
 */
MotorMap motor_map[MOTOR_MAP_SIZE] = {
    {"0", SERVO_0_GPIO_PIN, (SERVO_0_GPIO_PIN >> 1u) & 7u, SERVO_0_GPIO_PIN & 1u, SERVO_0_POWER_PIN, 0, 0, 0, 0, false},
    {"1", SERVO_1_GPIO_PIN, (SERVO_1_GPIO_PIN >> 1u) & 7u, SERVO_1_GPIO_PIN & 1u, SERVO_1_POWER_PIN, 0, 0, 0, 0, false},
    {"2", SERVO_2_GPIO_PIN, (SERVO_2_GPIO_PIN >> 1u) & 7u, SERVO_2_GPIO_PIN & 1u, SERVO_2_POWER_PIN, 0, 0, 0, 0, false},
    {"3", SERVO_3_GPIO_PIN, (SERVO_3_GPIO_PIN >> 1u) & 7u, SERVO_3_GPIO_PIN & 1u, SERVO_3_POWER_PIN, 0, 0, 0, 0, false},
    {"4", SERVO_4_GPIO_PIN, (SERVO_4_GPIO_PIN >> 1u) & 7u, SERVO_4_GPIO_PIN & 1u, SERVO_4_POWER_PIN, 0, 0, 0, 0, false},
    {"5", SERVO_5_GPIO_PIN, (SERVO_5_GPIO_PIN >> 1u) & 7u, SERVO_5_GPIO_PIN & 1u, SERVO_5_POWER_PIN, 0, 0, 0, 0, false},
    {"6", SERVO_6_GPIO_PIN, (SERVO_6_GPIO_PIN >> 1u) & 7u, SERVO_6_GPIO_PIN & 1u, SERVO_6_POWER_PIN, 0, 0, 0, 0, false},
    {"7", SERVO_7_GPIO_PIN, (SERVO_7_GPIO_PIN >> 1u) & 7u, SERVO_7_GPIO_PIN & 1u, SERVO_7_POWER_PIN, 0, 0, 0, 0,
     false}};

#ifdef CC_VER4
// Dynamixel motor map
DynamixelMotorEntry dxl_motors[MAX_DYNAMIXEL_SERVOS] = {0};
u8 dxl_motor_count = 0;

// HAL context for the Dynamixel bus
dxl_hal_context_t *dxl_ctx = NULL;

// Mutex for thread-safe access to dxl_motors
static SemaphoreHandle_t dxl_motors_mutex = NULL;
#endif

/**
 * The values we've read from the ADC for the position of our motors.
 *
 * Not every motor has a position sense pin, so it's not a good idea to assume
 * that all of the values in this array are valid.
 *
 * I debated back and forth with myself if I could include this information in
 * the `motor_map`, but decided not to. The reason is thread safety. The
 * firmware code isn't "multithreaded," so to speak, but Pi Picos have more that
 * on core, so it's possible that two "threads" could be touching things at
 * once. I did not want to have to deal with things that might light inside of
 * the ISR that runs to update the servos, so I decided to keep this separate.
 */
analog_filter sensed_motor_position[CONTROLLER_MOTORS_PER_MODULE];

/**
 * We need to know how long each frame is in microseconds. This is set when the
 * first servo is configured. It's _possible_ for the Pi Pico to run PWM channel
 * at a different frequency, but we don't do that. It doesn't really make sense
 * to do it, since almost all servos work at 50Hz.
 */
u64 frame_length_microseconds = 0UL;

/**
 * What's the size of the PWM counter?
 */
u32 pwm_resolution = 0UL;

/**
 * Have we been initialized by a computer?
 *
 * Don't run the control loop until we know it's safe to do so. We don't want to
 * accidentally break plastic.
 */
volatile bool controller_safe_to_run = false;

/**
 * A timer that gets fired to request that the computer we're connected to
 * send us servo information
 */
TimerHandle_t controller_init_request_timer = NULL;

/**
 * This timer is used to check if the controller is requesting
 * us to reset. Used to signal that the controller has restarted
 * and has a new config for us.
 *
 * This can also be accomplished by unplugging the USB port, but
 * if we're running in UART mode we don't have a way to know that
 * the controller has been restarted.
 */
TimerHandle_t controller_reset_request_check_timer = NULL;

/**
 * The current state of the firmware
 *
 * This is reflected in the LEDs on the board
 */
enum FirmwareState controller_firmware_state = idle;

/**
 * Keep track of if we've received the first frame
 *
 * This is set in the position handler
 */
volatile bool has_first_frame_been_received = false;

void controller_init() {
    info("init-ing the controller");

    // Create mutex for thread-safe access to motor_map
    motor_map_mutex = xSemaphoreCreateMutex();
    if (motor_map_mutex == NULL) {
        fatal("Failed to create motor_map_mutex");
        return;
    }

    // Create the analog filters for the sensed motor positions
    for (size_t i = 0; i < CONTROLLER_MOTORS_PER_MODULE; i++) {
        sensed_motor_position[i] = create_analog_filter(true, (float)ANALOG_READ_FILTER_SNAP_VALUE,
                                                        (float)ANALOG_READ_FILTER_ACTIVITY_THRESHOLD,
                                                        ANALOG_READ_FILTER_EDGE_SNAP_ENABLE == 1 ? true : false);
    }
    debug("created the analog filters for the sensed motor positions");

    // Create, but don't actually start the timer (it will be started when the
    // CDC is connected)
    controller_init_request_timer = xTimerCreate("Init Request Sender",               // Timer name
                                                 pdMS_TO_TICKS(INIT_REQUEST_TIME_MS), // Fire every INIT_REQUEST_TIME_MS
                                                 pdTRUE,                              // Auto-reload
                                                 (void *)0,                           // Timer ID (not used here)
                                                 send_init_request                    // Callback function
    );

    // If this fails, something is super broke. Bail out now.
    if (controller_init_request_timer == NULL) {
        fatal("Failed to create controller_init_request_timer");
        return;
    }

#ifdef CC_VER4
    // Initialize the Dynamixel motor map mutex
    dxl_motors_mutex = xSemaphoreCreateMutex();
    if (dxl_motors_mutex == NULL) {
        fatal("failed to create dxl_motors_mutex");
        return;
    }

    // Initialize the Dynamixel HAL
    dxl_hal_config_t dxl_config = {
        .data_pin = DXL_DATA_PIN,
        .baud_rate = DXL_BAUD_RATE,
        .pio = DXL_PIO,
    };
    dxl_ctx = dxl_hal_init(&dxl_config);
    if (dxl_ctx == NULL) {
        fatal("failed to initialize Dynamixel HAL");
        return;
    }
    info("Dynamixel HAL initialized on pin %u at %lu baud", DXL_DATA_PIN, (unsigned long)DXL_BAUD_RATE);

#endif

    // Set up the GPIO pin for monitoring for a reset signal
    gpio_set_function(CONTROLLER_RESET_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(CONTROLLER_RESET_PIN, GPIO_IN);

    // Create the timer that checks for a reset request
    controller_reset_request_check_timer =
        xTimerCreate("Reset Request Checker",                          // Timer name
                     pdMS_TO_TICKS(CONTROLLER_RESET_SIGNAL_PERIOD_MS), // Fire every
                                                                       // CONTROLLER_RESET_SIGNAL_PERIOD_MS
                     pdTRUE,                                           // Auto-reload
                     (void *)0,                                        // Timer ID (not used here)
                     controller_reset_request_check_timer_callback     // Callback function
        );

    // Same deal, this shouldn't happen.
    if (controller_reset_request_check_timer == NULL) {
        fatal("Failed to create controller_reset_request_check_timer");
        return;
    }
}

void controller_start() {
    info("starting the controller");

    // Fire up PWM
    u32 wrap = 0UL;
    for (size_t i = 0; i < sizeof(motor_map) / sizeof(motor_map[0]); ++i) {
        gpio_set_function(motor_map[i].gpio_pin, GPIO_FUNC_PWM);
        wrap = pwm_set_freq_duty(motor_map[i].slice, motor_map[i].channel, SERVO_FREQUENCY, 0);
        pwm_set_enabled(motor_map[i].slice, true);
    }

    /*
     * If this is the first one, set the frame length and resolution
     */
    if (frame_length_microseconds == 0UL) {
        frame_length_microseconds = 1000000UL / SERVO_FREQUENCY;
        pwm_resolution = wrap;
    }

    // Install the IRQ handler for the servos. Use servo 0 as a proxy for the
    // rest.
    pwm_set_irq_enabled(motor_map[0].slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap_handler);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Start the timer that checks for a request to reset from the controller
    xTimerStart(controller_reset_request_check_timer, 0);
}

u8 getMotorMapIndex(const char *motor_id) {
    if (motor_id == NULL || motor_id[0] == '\0') {
        warning("motor_id is null while getting motor map index");
        return INVALID_MOTOR_ID;
    }

    const u8 motor_number = motor_id[0] - '0'; // Convert '0', '1', ..., '7' to 0, 1, ..., 7

    // Make sure the controller requested a valid motor
    if (motor_number < 0 || motor_number >= CONTROLLER_MOTORS_PER_MODULE) {
        warning("Invalid motor ID: %s", motor_id);
        return INVALID_MOTOR_ID;
    }

    return motor_number;
}

bool requestServoPosition(const char *motor_id, const u16 requestedMicroseconds) {
    if (motor_id == NULL || motor_id[0] == '\0') {
        warning("motor_id is null while requesting servo position");
        return false;
    }

    // Get the index in the array
    const u8 motor_id_index = getMotorMapIndex(motor_id);
    if (motor_id_index == INVALID_MOTOR_ID) {
        warning("Invalid motor ID: %s", motor_id);
        return false;
    }

    bool result = false;

    // Take the mutex to ensure thread-safe access
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        // Make sure the motor is allowed to move to this position
        if (requestedMicroseconds < motor_map[motor_id_index].min_microseconds ||
            requestedMicroseconds > motor_map[motor_id_index].max_microseconds) {
            error("Invalid position requested for %s: %u (valid is: %u - %u)", motor_id, requestedMicroseconds,
                  motor_map[motor_id_index].min_microseconds, motor_map[motor_id_index].max_microseconds);
            xSemaphoreGive(motor_map_mutex);
            return false;
        }

        // Update the number of microseconds we're set to for the status lights
        // to use
        motor_map[motor_id_index].current_microseconds = requestedMicroseconds;

        // What percentage of the frame is going to be set to on?
        const double frame_active = (float)requestedMicroseconds / (float)frame_length_microseconds;

        // ...and what counter value is that?
        const u32 desired_ticks = (u32)((float)pwm_resolution * frame_active);

        verbose("Requested position for %s: %u ticks -> %u microseconds", motor_id, desired_ticks,
                requestedMicroseconds);
        motor_map[motor_id_index].requested_position = desired_ticks;

        result = true;
        xSemaphoreGive(motor_map_mutex);
    } else {
        warning("Failed to take motor_map_mutex in requestServoPosition");
    }

    return result;
}

bool configureServoMinMax(const char *motor_id, const u16 minMicroseconds, const u16 maxMicroseconds) {
    if (motor_id == NULL || motor_id[0] == '\0') {
        debug("motor_id is null while setting configureServoMinMax");
        return false;
    }

    // Get the index in the array
    const u8 motor_id_index = getMotorMapIndex(motor_id);
    if (motor_id_index == INVALID_MOTOR_ID) {
        warning("Invalid motor ID while configuring: %s", motor_id);
        return false;
    }

    bool result = false;

    // Take the mutex to ensure thread-safe access
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        motor_map[motor_id_index].min_microseconds = minMicroseconds;
        motor_map[motor_id_index].max_microseconds = maxMicroseconds;
        info("updated the motor map to allow motor %s to move between %u and "
             "%u microseconds",
             motor_id, minMicroseconds, maxMicroseconds);

        motor_map[motor_id_index].is_configured = true;

        result = true;
        xSemaphoreGive(motor_map_mutex);
    } else {
        warning("Failed to take motor_map_mutex in configureServoMinMax");
    }

    return result;
}

void __isr on_pwm_wrap_handler() {
    // This is an ISR. Treat with caution! ☠️

    // Using local variable to safely access volatile flag
    // In an ISR we want to be quick, so we'll just use a local copy
    // rather than a full critical section, since this is just reading a bool
    const bool is_safe = controller_safe_to_run;

    // Don't actually wiggle the motors if we haven't been told it's safe
    if (is_safe) {
        for (size_t i = 0; i < sizeof(motor_map) / sizeof(motor_map[0]); ++i) {
            pwm_set_chan_level(motor_map[i].slice, motor_map[i].channel, motor_map[i].requested_position);
        }
    }

    // Clear the IRQ regardless of if it's safe to wiggle things
    pwm_clear_irq(motor_map[0].slice);
    number_of_pwm_wraps = number_of_pwm_wraps + 1;

    // Update the watchdog every PWM_WRAPS_PER_WATCHDOG_UPDATE wraps
    watchdog_wrap_count++;
    if (watchdog_wrap_count >= PWM_WRAPS_PER_WATCHDOG_UPDATE) {
        watchdog_wrap_count = 0;
        watchdog_update();
    }
}

void send_init_request(TimerHandle_t xTimer) {

    // Avoid unused parameter warning
    (void)xTimer;

    char message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH] = {0};

    snprintf(message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH, "INIT\t%u", PROTOCOL_VERSION);

    send_to_controller(message);
    debug("sent init request");
}

u32 pwm_set_freq_duty(const u32 slice_num, const u32 chan, const u32 frequency, const int d) {
    const u32 clock = 125000000;
    u32 divider16 = clock / frequency / 4096 + (clock % (frequency * 4096) != 0);
    if (divider16 / 16 == 0)
        divider16 = 16;
    const u32 wrap = (clock << 4) / divider16 / frequency - 1; // Using bit shift for efficiency
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * d / 100);
    return wrap;
}

/**
 * Called from a timer in usb.c when the CDC is connected
 */
void controller_connected() {
    // We just got connected for the first time, halt anything
    // that might already be running
    controller_safe_to_run = false;

    // We're in state configuring now!
    controller_firmware_state = configuring;

    // Start sending init requests
    xTimerStart(controller_init_request_timer, 0);
    debug("started asking the computer for our configuration");
}

/**
 * Called from a timer in usb.c when the CDC is disconnected
 */
void controller_disconnected() {
    info("controller disconnected, stopping");
    controller_safe_to_run = false;

#ifdef CC_VER4
    dynamixel_set_torque_all(false);
#endif

    // Back to idle we go!
    controller_firmware_state = idle;

    // Flag that we've not gotten a good frame, but don't kill the relay
    has_first_frame_been_received = false;

    // No point in doing this if we're not connected
    xTimerStop(controller_init_request_timer, 0);
}

void firmware_configuration_received() {
    info("We've received a valid configuration from the controller!");

    // Tell everyone to go go go
    controller_firmware_state = running;
    controller_safe_to_run = true;

    // Let the controller know we're ready
    send_to_controller("READY\t1");
}

void first_frame_received(const bool yesOrNo) {
    has_first_frame_been_received = yesOrNo;

    if (yesOrNo) {
        info("We've received our first frame from the controller!");

#ifdef CC_VER3
        enable_all_motors();
#endif
#ifdef CC_VER4
        dynamixel_set_torque_all(true);
#endif
    } else {
        info("We haven't received our first frame from the controller yet");
#ifdef CC_VER3
        disable_all_motors();
#endif
#ifdef CC_VER4
        dynamixel_set_torque_all(false);
#endif
    }
}

void controller_reset_request_check_timer_callback(TimerHandle_t xTimer) {
    if (gpio_get(CONTROLLER_RESET_PIN)) {
        info("Controller reset request received");

        // If we're in the configuration state, there's nothing to do
        if (controller_firmware_state == configuring) {
            debug("doing nothing since we're in the configuring state");
            return;
        }

        // Go back to the configuring state
        controller_safe_to_run = false;
        controller_firmware_state = configuring;
        xTimerStart(controller_init_request_timer, 0);
        debug("started asking the computer for our configuration");
    }
}

/**
 * @brief Check if a motor is configured by the computer
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if motor is configured, false if not (or motor not found)
 */
bool is_motor_configured(const char *motor_id) {
    if (motor_id == NULL || motor_id[0] == '\0') {
        warning("motor_id is null while checking motor configuration");
        return false;
    }

    const u8 motor_index = getMotorMapIndex(motor_id);
    if (motor_index == INVALID_MOTOR_ID) {
        warning("invalid motor ID while checking configuration: %s", motor_id);
        return false;
    }

    bool configured = false;

    // Take the mutex to ensure thread-safe access
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        configured = motor_map[motor_index].is_configured;
        xSemaphoreGive(motor_map_mutex);
    } else {
        warning("failed to take motor_map_mutex in is_motor_configured");
    }

    return configured;
}

/**
 * @brief Check if all motors are configured
 *
 * @return true if all motors have been configured by the computer
 */
bool are_all_motors_configured(void) {
    bool all_configured = true;

    // Take the mutex to ensure thread-safe access
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < MOTOR_MAP_SIZE; i++) {
            if (!motor_map[i].is_configured) {
                all_configured = false;
                break;
            }
        }
        xSemaphoreGive(motor_map_mutex);
    } else {
        warning("failed to take motor_map_mutex in are_all_motors_configured");
        return false;
    }

    if (all_configured) {
        debug("all motors are configured");
    } else {
        warning("some motors still need configuration");
    }

    return all_configured;
}

void resetServoMotorMap(void) {
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < MOTOR_MAP_SIZE; i++) {
            motor_map[i].min_microseconds = 0;
            motor_map[i].max_microseconds = 0;
            motor_map[i].current_microseconds = 0;
            motor_map[i].requested_position = 0;
            motor_map[i].is_configured = false;
        }
        xSemaphoreGive(motor_map_mutex);
        debug("servo motor map reset");
    } else {
        warning("failed to take motor_map_mutex in resetServoMotorMap");
    }
}

#ifdef CC_VER4

void resetDynamixelMotorMap(void) {
    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        memset(dxl_motors, 0, sizeof(dxl_motors));
        dxl_motor_count = 0;
        xSemaphoreGive(dxl_motors_mutex);
        debug("Dynamixel motor map reset");
    } else {
        warning("failed to take dxl_motors_mutex in resetDynamixelMotorMap");
    }
}

bool configureDynamixelServo(u8 dxl_id, u32 min_pos, u32 max_pos, u32 profile_velocity) {
    if (dxl_ctx == NULL) {
        error("Dynamixel HAL not initialized");
        return false;
    }

    if (dxl_id == 0 || dxl_id > DXL_MAX_ID) {
        error("invalid Dynamixel ID: %u", dxl_id);
        return false;
    }

    bool result = false;

    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        if (dxl_motor_count >= MAX_DYNAMIXEL_SERVOS) {
            error("Dynamixel motor map full (%u max)", MAX_DYNAMIXEL_SERVOS);
            xSemaphoreGive(dxl_motors_mutex);
            return false;
        }

        // Check for duplicate ID
        for (u8 i = 0; i < dxl_motor_count; i++) {
            if (dxl_motors[i].dxl_id == dxl_id) {
                error("Dynamixel ID %u already configured", dxl_id);
                xSemaphoreGive(dxl_motors_mutex);
                return false;
            }
        }

        // Add entry
        DynamixelMotorEntry *entry = &dxl_motors[dxl_motor_count];
        entry->dxl_id = dxl_id;
        entry->min_position = min_pos;
        entry->max_position = max_pos;
        entry->requested_position = (min_pos + max_pos) / 2; // Center
        entry->is_configured = true;
        dxl_motor_count++;

        result = true;
        xSemaphoreGive(dxl_motors_mutex);
    } else {
        warning("failed to take dxl_motors_mutex in configureDynamixelServo");
        return false;
    }

    // Set Profile Velocity (outside mutex — HAL has its own synchronization)
    dxl_result_t dxl_res = dxl_set_profile_velocity(dxl_ctx, dxl_id, profile_velocity);
    if (dxl_res != DXL_OK) {
        warning("failed to set Profile Velocity for Dynamixel %u (result=%d)", dxl_id, dxl_res);
        // Non-fatal — the servo will still work, just with default velocity
    }

    info("configured Dynamixel servo %u: pos range [%lu-%lu], profile "
         "velocity %lu",
         dxl_id, (unsigned long)min_pos, (unsigned long)max_pos, (unsigned long)profile_velocity);

    return result;
}

bool requestDynamixelPosition(u8 dxl_id, u32 position) {
    bool result = false;

    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        for (u8 i = 0; i < dxl_motor_count; i++) {
            if (dxl_motors[i].dxl_id == dxl_id) {
                // Bounds check
                if (position < dxl_motors[i].min_position || position > dxl_motors[i].max_position) {
                    error("Dynamixel %u position %lu out of range [%lu-%lu]", dxl_id, (unsigned long)position,
                          (unsigned long)dxl_motors[i].min_position, (unsigned long)dxl_motors[i].max_position);
                    xSemaphoreGive(dxl_motors_mutex);
                    return false;
                }

                dxl_motors[i].requested_position = position;
                result = true;
                break;
            }
        }

        if (!result) {
            warning("Dynamixel ID %u not found in motor map", dxl_id);
        }

        xSemaphoreGive(dxl_motors_mutex);
    } else {
        warning("failed to take dxl_motors_mutex in requestDynamixelPosition");
    }

    return result;
}

void dynamixel_set_torque_all(bool enable) {
    if (dxl_ctx == NULL || dxl_motor_count == 0) {
        return;
    }

    info("%s torque on %u Dynamixel servos", enable ? "enabling" : "disabling", dxl_motor_count);

    // Read motor count/IDs under mutex, then release before bus operations
    u8 ids[MAX_DYNAMIXEL_SERVOS];
    u8 count = 0;

    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        count = dxl_motor_count;
        for (u8 i = 0; i < count; i++) {
            ids[i] = dxl_motors[i].dxl_id;
        }
        xSemaphoreGive(dxl_motors_mutex);
    }

    for (u8 i = 0; i < count; i++) {
        dxl_result_t res = dxl_set_torque(dxl_ctx, ids[i], enable);
        if (res != DXL_OK) {
            warning("failed to %s torque on Dynamixel %u (result=%d)", enable ? "enable" : "disable", ids[i], res);
        }

        // LED follows torque state
        dxl_result_t led_res = dxl_set_led(dxl_ctx, ids[i], enable);
        if (led_res != DXL_OK) {
            warning("failed to set LED on Dynamixel %u (result=%d)", ids[i], led_res);
        }
    }
}

portTASK_FUNCTION(dynamixel_controller_task, pvParameters) {
    (void)pvParameters;

    info("Dynamixel controller task started");

    const TickType_t frame_period = pdMS_TO_TICKS(20); // 50Hz
    u32 frame_counter = 0;

    // Buffers for Sync Write and Sync Read
    dxl_sync_position_t sync_positions[MAX_DYNAMIXEL_SERVOS];
    dxl_sync_status_result_t sync_results[MAX_DYNAMIXEL_SERVOS];
    u8 sync_ids[MAX_DYNAMIXEL_SERVOS];

    for (EVER) {
        TickType_t wake_time = xTaskGetTickCount();

        if (controller_safe_to_run && dxl_motor_count > 0) {
            u8 count = 0;

            // Build position array under mutex
            if (xSemaphoreTake(dxl_motors_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                count = dxl_motor_count;
                for (u8 i = 0; i < count; i++) {
                    sync_positions[i].id = dxl_motors[i].dxl_id;
                    sync_positions[i].position = dxl_motors[i].requested_position;
                    sync_ids[i] = dxl_motors[i].dxl_id;
                }
                xSemaphoreGive(dxl_motors_mutex);
            }

            // Sync Write positions — broadcast, no response expected (~0.5ms)
            if (count > 0) {
                dxl_sync_write_position(dxl_ctx, sync_positions, count);
            }

            // Periodic telemetry read
            if (count > 0 && frame_counter % DXL_SENSOR_REPORT_INTERVAL_FRAMES == 0) {
                u8 result_count = 0;
                dxl_result_t res = dxl_sync_read_status(dxl_ctx, sync_ids, count, sync_results, &result_count);

                if (res == DXL_OK && result_count > 0) {
                    // Build and send DSENSE message
                    char dsense_msg[OUTGOING_MESSAGE_MAX_LENGTH] = {0};
                    int offset = snprintf(dsense_msg, sizeof(dsense_msg), "DSENSE");

                    for (u8 i = 0; i < result_count && offset < (int)sizeof(dsense_msg) - 30; i++) {
                        if (sync_results[i].valid) {
                            // Convert voltage from Dynamixel units (0.1V) to mV
                            u32 voltage_mv = (u32)sync_results[i].status.present_voltage * 100;

                            // Convert temperature from Celsius to Fahrenheit
                            double temp_f = (double)sync_results[i].status.present_temperature * 9.0 / 5.0 + 32.0;

                            offset += snprintf(dsense_msg + offset, sizeof(dsense_msg) - offset, "\tD%u %.1f %d %lu",
                                               sync_results[i].id, temp_f,
                                               sync_results[i].status.present_load, (unsigned long)voltage_mv);

                            // Check for hardware errors
                            if (sync_results[i].servo_error != 0) {
                                warning("Dynamixel %u reports hardware error: 0x%02X", sync_results[i].id,
                                        sync_results[i].servo_error);
                            }
                        }
                    }

                    send_to_controller(dsense_msg);
                }
            }

            frame_counter++;
        }

        vTaskDelayUntil(&wake_time, frame_period);
    }
}

const dxl_metrics_t *controller_get_dxl_metrics(void) {
    if (dxl_ctx == NULL) {
        return NULL;
    }
    return dxl_hal_metrics(dxl_ctx);
}

#endif
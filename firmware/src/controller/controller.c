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
#include "watchdog/watchdog.h"

#include "config.h"
#include "controller.h"
#include "types.h"
#include "version.h"

#ifdef CC_VER4
#include "dynamixel/dynamixel_hal.h"
#include "dynamixel/dynamixel_registers.h"
#include "dynamixel/dynamixel_servo.h"
#include "messaging/processors/emergency_stop_message.h"
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

    // Refresh the watchdog every PWM_WRAPS_PER_WATCHDOG_UPDATE wraps.
    // watchdog_feed() applies the health gate (see USE_WATCHDOG_HEALTH_GATE):
    // it refreshes only if the scheduler proved liveness, so a hung scheduler
    // resets the board instead of this ISR petting it forever.
    watchdog_wrap_count++;
    if (watchdog_wrap_count >= PWM_WRAPS_PER_WATCHDOG_UPDATE) {
        watchdog_wrap_count = 0;
        watchdog_feed();
    }
}

void send_init_request(TimerHandle_t xTimer) {

    // Avoid unused parameter warning
    (void)xTimer;

    char message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH] = {0};

    // Report our hardware version (3 = standard servos only, 4 = adds Dynamixel)
    // so the host can configure us for what this board can actually drive.
    snprintf(message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH, "INIT\t%u", CREATURE_HARDWARE_VERSION);

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
    dynamixel_request_torque_all(false);
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
        dynamixel_request_torque_all(true);
#endif
    } else {
        info("We haven't received our first frame from the controller yet");
#ifdef CC_VER3
        disable_all_motors();
#endif
#ifdef CC_VER4
        dynamixel_request_torque_all(false);
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

// Whether the incoming motor power rail is currently up. Starts true so a board
// that boots with the motors already powered does not see a spurious "restored"
// edge on the very first reading. The Dynamixel task reads this to avoid talking
// to servos that have no power.
static volatile bool motor_power_rail_present = true;

// Set when servo configuration still needs to be applied to the bus — either the
// motor power rail returned after being cut, or a CONFIG arrived while the rail
// was down. Signals the Dynamixel task to (re-)apply servo configuration.
static volatile bool dxl_reinit_requested = false;

/*
 * Pending torque change, serviced by the Dynamixel task.
 *
 * The task owns the bus and the velocity_confirmed state machine, and it must
 * own them alone. Callers on other tasks used to drive torque directly, which
 * put two tasks through the same multi-step sequence at once: a re-initialize
 * clears every confirmed flag and re-establishes them one servo at a time, so a
 * caller reading those flags mid-flight would decide servos were unconfigured
 * and drive them torque-off underneath it. The bus mutex serializes individual
 * transactions; it cannot make a whole sequence atomic. Requesting the change
 * and letting the task perform it does.
 */
typedef enum {
    DXL_TORQUE_REQUEST_NONE = 0,
    DXL_TORQUE_REQUEST_ENABLE,
    DXL_TORQUE_REQUEST_DISABLE,
} dxl_torque_request_t;

static volatile dxl_torque_request_t dxl_torque_request = DXL_TORQUE_REQUEST_NONE;

static void dynamixel_set_torque_all(bool enable);
static bool dynamixel_apply_led(u8 dxl_id, bool on);

// Running tally of the Dynamixel telemetry path, reported periodically by the
// Dynamixel task. Both failure modes are otherwise silent from this end: a bus
// read that returns nothing, and a report that is built but dropped because the
// outgoing queue is full. Owned entirely by the Dynamixel task.
static u32 dsense_messages_sent = 0;
static u32 dsense_sends_dropped = 0;
static u32 dsense_reads_failed = 0;

void resetDynamixelMotorMap(void) {
    // Any torque request still queued refers to a configuration that is about to
    // cease to exist, so drop it rather than let it be applied to the new map.
    dxl_torque_request = DXL_TORQUE_REQUEST_NONE;

    // Let go of the servos before we forget they exist. Once the map is cleared
    // there is no list of IDs left to command, so anything still holding torque
    // would keep holding it — with settings from a configuration we are in the
    // middle of throwing away.
    //
    // This one is done inline rather than handed to the Dynamixel task: the
    // release has to happen while the servo IDs are still known, and the task
    // could not run it until after this function had already cleared them.
    // Configuration is processed with controller_safe_to_run false, so the
    // task's control sequence is idle while this runs.
    dynamixel_set_torque_all(false);

    // The status lights mean "settings verified" (issue #18), and the settings
    // they vouch for are about to be thrown away — go dark until the next
    // CONFIG re-verifies. Inline for the same reason as the torque release
    // above: once the map clears, the IDs to command are gone. Servos that
    // reappear in the new configuration get their light rewritten either way;
    // this sweep is for the ones that don't.
    if (motor_power_rail_present) {
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
            dynamixel_apply_led(ids[i], false);
        }
    }

    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        memset(dxl_motors, 0, sizeof(dxl_motors));
        dxl_motor_count = 0;
        xSemaphoreGive(dxl_motors_mutex);
        debug("Dynamixel motor map reset");
    } else {
        warning("failed to take dxl_motors_mutex in resetDynamixelMotorMap");
    }
}

/**
 * Write a servo register and read it back to confirm it actually landed,
 * retrying a few times with a short pause between attempts.
 *
 * A servo that is still booting after a power cycle may not answer at all, or
 * may answer before it has applied the write, so the read-back is what we trust
 * rather than the write's return code.
 *
 * Must be called from a task context (it delays between attempts).
 *
 * @param dxl_id servo to write to
 * @param address register address
 * @param length register width in bytes
 * @param value value to write
 * @param what what we are setting, for the log
 * @return true if the servo is confirmed to hold the requested value
 */
static bool dynamixel_apply_register(u8 dxl_id, u16 address, u16 length, u32 value, const char *what) {
    for (u8 attempt = 1; attempt <= DXL_WRITE_MAX_ATTEMPTS; attempt++) {
        // The rail can drop out mid-sequence (brownout, a plug switched off
        // between servos). Retrying into a dead bus just buys timeouts per
        // servo, so stop as soon as we know the power is gone.
        if (!motor_power_rail_present) {
            warning("motor power rail went down while setting %s on Dynamixel %u; abandoning the attempt", what,
                    dxl_id);
            return false;
        }

        dxl_result_t res = dxl_write_register(dxl_ctx, dxl_id, address, length, value);

        if (res == DXL_OK) {
            u32 readback = 0;
            res = dxl_read_register(dxl_ctx, dxl_id, address, length, &readback);

            if (res == DXL_OK && readback == value) {
                if (attempt > 1) {
                    info("%s on Dynamixel %u confirmed on attempt %u", what, dxl_id, attempt);
                }
                return true;
            }

            if (res == DXL_OK) {
                warning("Dynamixel %u %s read back as %lu, expected %lu (attempt %u of %u)", dxl_id, what,
                        (unsigned long)readback, (unsigned long)value, attempt, DXL_WRITE_MAX_ATTEMPTS);
            } else {
                warning("failed to read back %s for Dynamixel %u (%s, attempt %u of %u)", what, dxl_id,
                        dxl_result_to_string(res), attempt, DXL_WRITE_MAX_ATTEMPTS);
            }
        } else {
            warning("failed to set %s on Dynamixel %u (%s, attempt %u of %u)", what, dxl_id, dxl_result_to_string(res),
                    attempt, DXL_WRITE_MAX_ATTEMPTS);
        }

        if (attempt < DXL_WRITE_MAX_ATTEMPTS) {
            vTaskDelay(pdMS_TO_TICKS(DXL_WRITE_RETRY_DELAY_MS));
        }
    }

    error("giving up on %s for Dynamixel %u after %u attempts", what, dxl_id, DXL_WRITE_MAX_ATTEMPTS);
    return false;
}

static bool dynamixel_apply_profile_velocity(u8 dxl_id, u32 velocity) {
    return dynamixel_apply_register(dxl_id, DXL_REG_PROFILE_VELOCITY, 4, velocity, "Profile Velocity");
}

/*
 * A torque enable that quietly fails leaves a servo limp while the rest of the
 * system believes it is holding position, and a torque disable that quietly
 * fails leaves it energized when we wanted it released — the more dangerous of
 * the two. Both are worth confirming.
 */
static bool dynamixel_apply_torque(u8 dxl_id, bool enable) {
    return dynamixel_apply_register(dxl_id, DXL_REG_TORQUE_ENABLE, 1, enable ? 1u : 0u,
                                    enable ? "torque enable" : "torque disable");
}

/*
 * The status light means one thing: this servo's settings were written and read
 * back (issue #18). It comes on at the end of verification — initial CONFIG or
 * the re-init after a power restore — before any torque is applied, so standing
 * at the rig you can see that configuration landed without waiting for frames.
 * It says nothing about torque: a lit servo may well be limp.
 */
static bool dynamixel_apply_led(u8 dxl_id, bool on) {
    return dynamixel_apply_register(dxl_id, DXL_REG_LED, 1, on ? 1u : 0u, on ? "status light on" : "status light off");
}

/**
 * Record whether a servo's Profile Velocity is confirmed, so torque enable can
 * skip the ones we could not configure.
 *
 * Looks the servo up by ID rather than by index because the caller releases the
 * motor map lock to do its bus work. Today the only writers to the map
 * (configureDynamixelServo, resetDynamixelMotorMap) both run on the inbound
 * message chain and so cannot interleave with each other; if that ever changes,
 * a servo that disappeared from the map mid-write is simply not marked.
 */
static void dynamixel_mark_velocity_confirmed(u8 dxl_id, bool confirmed) {
    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        for (u8 i = 0; i < dxl_motor_count; i++) {
            if (dxl_motors[i].dxl_id == dxl_id) {
                dxl_motors[i].velocity_confirmed = confirmed;
                break;
            }
        }
        xSemaphoreGive(dxl_motors_mutex);
    } else {
        warning("failed to take dxl_motors_mutex in dynamixel_mark_velocity_confirmed");
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

    // A velocity above the servo's limit would be rejected or silently clamped,
    // and we would then never be able to confirm it — which now costs us torque
    // on that servo. Clamp here so an over-range config does not leave it limp.
    if (profile_velocity > DXL_MAX_PROFILE_VELOCITY) {
        warning("Profile Velocity %lu for Dynamixel %u exceeds the maximum of %u, clamping",
                (unsigned long)profile_velocity, dxl_id, DXL_MAX_PROFILE_VELOCITY);
        profile_velocity = DXL_MAX_PROFILE_VELOCITY;
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
        entry->profile_velocity = profile_velocity;          // Retained for re-init after a power cycle
        entry->is_configured = true;
        entry->velocity_confirmed = false; // Set below, once the servo reads it back
        dxl_motor_count++;

        result = true;
        xSemaphoreGive(dxl_motors_mutex);
    } else {
        warning("failed to take dxl_motors_mutex in configureDynamixelServo");
        return false;
    }

    // Set Profile Velocity (outside mutex — HAL has its own synchronization).
    // If the motor power rail is down there is nothing on the bus to answer us,
    // so skip the writes rather than spending three timeouts per servo proving
    // what we already know. The settings are applied when the rail comes back.
    bool confirmed = false;

    if (motor_power_rail_present) {
        confirmed = dynamixel_apply_profile_velocity(dxl_id, profile_velocity);
        dynamixel_mark_velocity_confirmed(dxl_id, confirmed);

        // Settings verified — light the servo's status LED (issue #18). Off on a
        // failed confirm, in case it was lit by a previous configuration.
        if (!dynamixel_apply_led(dxl_id, confirmed)) {
            warning("Dynamixel %u status light may not match its verification state", dxl_id);
        }
    } else {
        dynamixel_mark_velocity_confirmed(dxl_id, false);
        dxl_reinit_requested = true;
    }

    info("configured Dynamixel servo %u: pos range [%lu-%lu], profile "
         "velocity %lu (%s)",
         dxl_id, (unsigned long)min_pos, (unsigned long)max_pos, (unsigned long)profile_velocity,
         confirmed                  ? "confirmed"
         : motor_power_rail_present ? "UNCONFIRMED, torque will stay off"
                                    : "deferred, motor power rail is down — will be applied when power returns");

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

void dynamixel_request_torque_all(bool enable) {
    dxl_torque_request = enable ? DXL_TORQUE_REQUEST_ENABLE : DXL_TORQUE_REQUEST_DISABLE;
    debug("requested Dynamixel torque %s", enable ? "enable" : "disable");
}

// Apply a torque change to every configured servo. Runs on the Dynamixel task
// only — everyone else goes through dynamixel_request_torque_all().
static void dynamixel_set_torque_all(bool enable) {
    if (dxl_ctx == NULL || dxl_motor_count == 0) {
        return;
    }

    // Servos with no power hold no torque, and nothing on the bus will answer.
    // The re-init that follows the rail coming back sets torque appropriately.
    if (!motor_power_rail_present) {
        info("skipping %s torque on %u Dynamixel servos: motor power rail is down", enable ? "enabling" : "disabling",
             dxl_motor_count);
        return;
    }

    info("%s torque on %u Dynamixel servos", enable ? "enabling" : "disabling", dxl_motor_count);

    // Read motor count/IDs under mutex, then release before bus operations
    u8 ids[MAX_DYNAMIXEL_SERVOS];
    bool confirmed[MAX_DYNAMIXEL_SERVOS];
    u8 count = 0;

    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        count = dxl_motor_count;
        for (u8 i = 0; i < count; i++) {
            ids[i] = dxl_motors[i].dxl_id;
            confirmed[i] = dxl_motors[i].velocity_confirmed;
        }
        xSemaphoreGive(dxl_motors_mutex);
    }

    u8 energized = 0; // confirmed holding torque
    u8 released = 0;  // confirmed released, whether asked for or withheld
    u8 failed = 0;    // could not be confirmed either way

    for (u8 i = 0; i < count; i++) {
        // A servo we could not confirm the settings on stays limp — moving at
        // an unknown speed is worse than not moving at all. Drive it torque-off
        // explicitly rather than just skipping it, in case it is still holding
        // torque from before.
        bool want_torque = enable && confirmed[i];

        if (enable && !confirmed[i]) {
            error("leaving Dynamixel %u torque-off: its Profile Velocity was never confirmed", ids[i]);
        }

        const bool torque_ok = dynamixel_apply_torque(ids[i], want_torque);

        if (torque_ok) {
            if (want_torque) {
                energized++;
            } else {
                released++;
            }
        } else {
            failed++;
        }

        // The status light is deliberately not touched here: it means "settings
        // verified", not "holding torque" (issue #18), and verification state
        // doesn't change on a torque transition.
    }

    // State the outcome plainly, counting what each servo actually ended up
    // doing rather than how many writes landed. A servo held off on purpose is
    // not an enabled servo, and a servo that would not take a torque enable is
    // limp while everything else assumes it is holding.
    if (enable) {
        if (energized == count) {
            info("torque enabled on all %u Dynamixel servos", count);
        } else {
            error("torque enabled on %u of %u Dynamixel servos (%u held off, %u could not be confirmed)", energized,
                  count, released, failed);
        }
    } else {
        if (released == count) {
            info("torque disabled on all %u Dynamixel servos", count);
        } else {
            error("torque disabled on %u of %u Dynamixel servos (%u could not be confirmed)", released, count, failed);
        }
    }
}

/**
 * Build and send a DSENSE report.
 *
 * Every configured servo appears in every report, with a trailing online flag.
 * A servo we did not hear from — because it failed to answer, or because the
 * whole bus has no power — reports zeros with the flag clear. Omitting it
 * instead, as this used to do, leaves the controller showing the last good
 * reading as though it were current, which is worse than showing nothing.
 *
 * @param ids servo IDs to report on, in motor map order
 * @param id_count how many entries ids holds
 * @param results sync read results, or NULL when the bus was not read at all
 * @param result_count how many entries results holds
 */
static void dynamixel_send_sensor_report(const u8 *ids, u8 id_count, const dxl_sync_status_result_t *results,
                                         u8 result_count) {
    char dsense_msg[OUTGOING_MESSAGE_MAX_LENGTH] = {0};
    int offset = snprintf(dsense_msg, sizeof(dsense_msg), "DSENSE");

    for (u8 i = 0; i < id_count && offset < (int)sizeof(dsense_msg) - DXL_SENSOR_REPORT_TOKEN_MAX; i++) {
        const dxl_sync_status_result_t *reading = NULL;

        for (u8 j = 0; j < result_count; j++) {
            if (results[j].id == ids[i] && results[j].valid) {
                reading = &results[j];
                break;
            }
        }

        if (reading != NULL) {
            // Convert voltage from Dynamixel units (0.1V) to mV
            u32 voltage_mv = (u32)reading->status.present_voltage * 100;

            // Convert temperature from Celsius to Fahrenheit
            double temp_f = (double)reading->status.present_temperature * 9.0 / 5.0 + 32.0;

            // Token: D<id> <temp_F> <load> <voltage_mV> <position> <online>
            offset += snprintf(dsense_msg + offset, sizeof(dsense_msg) - offset, "\tD%u %.1f %d %lu %ld 1", ids[i],
                               temp_f, reading->status.present_load, (unsigned long)voltage_mv,
                               (long)reading->status.present_position);

            // Check for hardware errors
            if (reading->servo_error != 0) {
                warning("Dynamixel %u reports hardware error: 0x%02X", ids[i], reading->servo_error);
            }
        } else {
            offset += snprintf(dsense_msg + offset, sizeof(dsense_msg) - offset, "\tD%u 0.0 0 0 0 0", ids[i]);
        }
    }

    // Track the send result rather than discarding it: a full outgoing queue
    // drops the report just as effectively as a failed bus read, and the two
    // want different fixes.
    if (send_to_controller(dsense_msg)) {
        dsense_messages_sent++;
    } else {
        dsense_sends_dropped++;
    }
}

// Re-apply Dynamixel configuration after the motor power rail was cycled. The
// servos come back with torque disabled and their Profile Velocity register at
// the factory default, so restore the velocity we were configured with, confirm
// it took, and then re-enable torque on the servos we could confirm. Runs on
// the Dynamixel control task, which owns the bus.
static void dynamixel_reinitialize_all(void) {
    if (dxl_ctx == NULL || dxl_motor_count == 0) {
        return;
    }

    info("re-initializing %u Dynamixel servos after motor power restore", dxl_motor_count);

    // Snapshot ids and velocities under the mutex, then do bus ops without it
    // held. Everything the servos knew died with the power rail, so nothing is
    // confirmed until it has been written and read back again.
    u8 ids[MAX_DYNAMIXEL_SERVOS];
    u32 velocities[MAX_DYNAMIXEL_SERVOS];
    u8 count = 0;

    if (xSemaphoreTake(dxl_motors_mutex, portMAX_DELAY) == pdTRUE) {
        count = dxl_motor_count;
        for (u8 i = 0; i < count; i++) {
            ids[i] = dxl_motors[i].dxl_id;
            velocities[i] = dxl_motors[i].profile_velocity;
            dxl_motors[i].velocity_confirmed = false;
        }
        xSemaphoreGive(dxl_motors_mutex);
    }

    // Restore each servo's Profile Velocity (lost on the power cycle), using the
    // same write-and-verify path as initial configuration
    u8 confirmed_count = 0;
    for (u8 i = 0; i < count; i++) {
        bool confirmed = dynamixel_apply_profile_velocity(ids[i], velocities[i]);
        dynamixel_mark_velocity_confirmed(ids[i], confirmed);

        // The LED register reset with the rail, so relight it as each servo's
        // settings are re-verified (issue #18).
        if (!dynamixel_apply_led(ids[i], confirmed)) {
            warning("Dynamixel %u status light may not match its verification state", ids[i]);
        }

        if (confirmed) {
            confirmed_count++;
        }
    }

    // Say what happened either way. A clean run is otherwise silent — the
    // per-servo lines only appear on a retry or a failure — which makes "every
    // servo confirmed" and "the whole loop was skipped" look identical in the
    // log of an init path that really needs to be readable.
    if (confirmed_count == count) {
        info("re-initialized %u of %u Dynamixel servos (all confirmed)", confirmed_count, count);
    } else {
        error("re-initialized %u of %u Dynamixel servos; %u will stay torque-off", confirmed_count, count,
              (u8)(count - confirmed_count));
    }

    // Torque and status LED back on, for the servos we could confirm — but only
    // if the creature is supposed to be moving at all. An emergency stop halts
    // the message processor, not this task, so without this check a power rail
    // blip would quietly re-arm a creature that was deliberately stopped. The
    // first-frame check keeps us from energizing servos to their centered
    // defaults before the controller has actually commanded a position.
    if (is_emergency_stop_active()) {
        error("not enabling Dynamixel torque after power restore: emergency stop is active");
        return;
    }

    if (!has_first_frame_been_received) {
        info("not enabling Dynamixel torque after power restore: no frames from the controller yet");
        return;
    }

    dynamixel_set_torque_all(true);
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

        // Service any torque request from another task. This sits outside the
        // safe-to-run gate on purpose: a disconnect clears that flag before
        // asking for torque off, and releasing the servos is precisely what we
        // still have to do.
        dxl_torque_request_t torque_request = dxl_torque_request;
        if (torque_request != DXL_TORQUE_REQUEST_NONE) {
            dxl_torque_request = DXL_TORQUE_REQUEST_NONE;

            if (torque_request == DXL_TORQUE_REQUEST_ENABLE && is_emergency_stop_active()) {
                error("ignoring Dynamixel torque enable request: emergency stop is active");
            } else if (torque_request == DXL_TORQUE_REQUEST_ENABLE && dxl_reinit_requested) {
                // A re-initialize is already queued and ends by enabling torque
                // itself. Let it do the work with freshly confirmed settings
                // instead of racing ahead using stale ones.
                debug("deferring torque enable to the pending re-initialize");
            } else {
                dynamixel_set_torque_all(torque_request == DXL_TORQUE_REQUEST_ENABLE);
            }
        }

        if (controller_safe_to_run && dxl_motor_count > 0 && !motor_power_rail_present) {
            // Rail is down, so no bus traffic - there is no point talking to
            // servos that have no power, and it only racks up timeouts. Keep
            // reporting them as offline, though, or the controller goes on
            // showing the last good readings as if they were current. Nothing
            // can change until power returns, so report at a slower cadence.
            if (frame_counter % DXL_SENSOR_REPORT_OFFLINE_INTERVAL_FRAMES == 0) {
                u8 offline_ids[MAX_DYNAMIXEL_SERVOS];
                u8 offline_count = 0;

                if (xSemaphoreTake(dxl_motors_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    offline_count = dxl_motor_count;
                    for (u8 i = 0; i < offline_count; i++) {
                        offline_ids[i] = dxl_motors[i].dxl_id;
                    }
                    xSemaphoreGive(dxl_motors_mutex);
                }

                if (offline_count > 0) {
                    dynamixel_send_sensor_report(offline_ids, offline_count, NULL, 0);
                }
            }

            frame_counter++;
        }

        // Normal operation, with the rail up. Work resumes here (starting with a
        // re-init) once power returns.
        if (controller_safe_to_run && dxl_motor_count > 0 && motor_power_rail_present) {
            u8 count = 0;

            // If the motor power rail was cycled, the servos came back torque-off
            // with a default Profile Velocity. Re-apply their config before
            // resuming normal control. Cleared before the work so a transition
            // that arrives mid-re-init is caught on the next pass.
            if (dxl_reinit_requested) {
                dxl_reinit_requested = false;
                dynamixel_reinitialize_all();
            }

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

            // Periodic telemetry read. The report goes out whether or not the
            // read succeeded — servos that did not answer are flagged offline
            // rather than left out, so every configured servo is accounted for
            // in every report.
            if (count > 0 && frame_counter % DXL_SENSOR_REPORT_INTERVAL_FRAMES == 0) {
                u8 result_count = 0;
                dxl_result_t res = dxl_sync_read_status(dxl_ctx, sync_ids, count, sync_results, &result_count);

                if (res != DXL_OK || result_count == 0) {
                    dsense_reads_failed++;
                }

                dynamixel_send_sensor_report(sync_ids, count, sync_results, (res == DXL_OK) ? result_count : 0);
            }

            // Periodic tally so a stalled telemetry path is visible from here
            if (frame_counter % DXL_SENSOR_REPORT_STATS_INTERVAL_FRAMES == 0) {
                info("DSENSE telemetry: %lu sent, %lu dropped, %lu reads failed", (unsigned long)dsense_messages_sent,
                     (unsigned long)dsense_sends_dropped, (unsigned long)dsense_reads_failed);
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

void controller_motor_power_sample(float voltage) {
    if (motor_power_rail_present) {
        // Watch for the rail dropping out (smart plug switched off, etc.)
        if (voltage < MOTOR_POWER_RAIL_LOST_VOLTAGE) {
            motor_power_rail_present = false;
            info("motor power rail lost (%.2fV)", voltage);
        }
    } else {
        // Watch for the rail coming back up
        if (voltage > MOTOR_POWER_RAIL_RESTORED_VOLTAGE) {
            motor_power_rail_present = true;
            info("motor power rail restored (%.2fV)", voltage);

            // Always ask for a re-init. Anything configured while the rail was
            // down was deferred rather than written, so the servos need their
            // settings either way. The Dynamixel task holds the request until
            // it is safe to run and there are motors to configure.
            dxl_reinit_requested = true;
        }
    }
}

#endif

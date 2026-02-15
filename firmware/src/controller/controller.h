#pragma once

#include <limits.h>

// Our modules
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <timers.h>

#include "config.h"

/**
 * @brief The maximum number of motors per module
 *
 * This defines how many individual servos we can control per module.
 * Currently set to 8 (numbered 0-7).
 */
#define CONTROLLER_MOTORS_PER_MODULE 8

/**
 * @brief Total number of motors in the system
 *
 * This calculates the total number of motors across all modules.
 * Currently we have 1 module with CONTROLLER_MOTORS_PER_MODULE servos.
 */
#define MOTOR_MAP_SIZE (1 * CONTROLLER_MOTORS_PER_MODULE)

/**
 * @brief Constant representing an invalid motor ID
 *
 * Used when a motor cannot be found or an operation fails.
 */
#define INVALID_MOTOR_ID UINT8_MAX

/**
 * @brief Constant representing an invalid motor index
 *
 * Used when a motor index is out of bounds or invalid.
 */
#define INVALID_MOTOR_INDEX UINT16_MAX

/**
 * @brief Mutex for thread-safe access to motor_map
 *
 * This mutex protects the motor_map data structure from concurrent access
 * by multiple tasks or cores, preventing race conditions.
 */
extern SemaphoreHandle_t motor_map_mutex;

/**
 * @brief Structure mapping a motor ID to its hardware configuration
 *
 * This structure contains all information needed to control a servo motor,
 * including GPIO pin, PWM slice/channel, position limits, current state,
 * and power control.
 */
typedef struct {
    const char *motor_id;     /**< String identifier for this motor (e.g., "0", "1") */
    u32 gpio_pin;             /**< GPIO pin number on the Pi Pico */
    u32 slice;                /**< PWM slice used by this motor */
    u32 channel;              /**< PWM channel within the slice */
    u32 power_pin;            /**< GPIO pin controlling power to this motor */
    u16 requested_position;   /**< Position in PWM counter ticks (not
                                 microseconds) */
    u16 min_microseconds;     /**< Minimum pulse width in microseconds */
    u16 max_microseconds;     /**< Maximum pulse width in microseconds */
    u16 current_microseconds; /**< Current pulse width in microseconds */
    bool is_configured;       /**< True if this motor has been configured by the
                                 computer */
} MotorMap;

/**
 * @brief Firmware state enumeration
 *
 * Represents the current operating state of the controller firmware.
 * This state is reflected in the status LEDs.
 */
enum FirmwareState {
    idle,        /**< Not active, waiting for commands */
    configuring, /**< Receiving configuration from host computer */
    running,     /**< Normal operation mode */
    errored_out  /**< An error occurred that prevents normal operation */
};

/**
 * @brief Converts a motor ID string to an index in the motor_map array
 *
 * @param motor_id The motor ID string to convert (e.g., "0", "1", etc.)
 * @return The corresponding index in the motor_map array, or INVALID_MOTOR_ID
 * if not found
 */
u8 getMotorMapIndex(const char *motor_id);

/**
 * @brief Interrupt Service Routine for PWM wrap events
 *
 * This ISR is called when the PWM counter wraps around, which happens at the
 * start of each PWM cycle. It updates the PWM duty cycle for all servos.
 */
void __isr on_pwm_wrap_handler();

/**
 * @brief Initialize the controller subsystem
 *
 * Sets up mutexes, analog filters, timers, and GPIO pins needed for the
 * controller operation. Must be called before controller_start().
 */
void controller_init();

/**
 * @brief Start the controller operation
 *
 * Configures PWM for all servos, sets up the frame timing, and installs
 * the PWM interrupt handler. Call after controller_init().
 */
void controller_start();

/**
 * @brief Request that a servo move to a specific position
 *
 * Sets a servo's position by specifying the pulse width in microseconds.
 * The function validates that the requested position is within the
 * configured min/max limits for the servo before applying it.
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @param requestedMicroseconds The pulse width in microseconds (typically
 * 1000-2000)
 * @return true if the position was set successfully, false if there was an
 * error
 */
bool requestServoPosition(const char *motor_id, u16 requestedMicroseconds);

/**
 * @brief Configure the minimum and maximum position limits for a servo
 *
 * Sets the valid range of motion for a servo in microseconds. These limits
 * are used by requestServoPosition() to prevent commanding positions that
 * could damage the mechanical system.
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @param minMicroseconds The minimum allowed pulse width in microseconds
 * @param maxMicroseconds The maximum allowed pulse width in microseconds
 * @return true if configuration was successful, false if there was an error
 */
bool configureServoMinMax(const char *motor_id, u16 minMicroseconds, u16 maxMicroseconds);

/**
 * @brief Configure PWM frequency and duty cycle for a channel
 *
 * Sets up a PWM channel with the specified frequency and duty cycle.
 * Calculates appropriate clock dividers and wrap values for the Pi Pico PWM
 * hardware.
 *
 * @param slice_num The PWM slice number
 * @param chan The PWM channel number within the slice
 * @param frequency The desired PWM frequency in Hz (typically 50Hz for servos)
 * @param d The duty cycle as a percentage (0-100)
 * @return The wrap value (counter resolution) configured for the PWM channel
 */
u32 pwm_set_freq_duty(u32 slice_num, u32 chan, u32 frequency, int d);

/**
 * @brief Handle a new controller connection
 *
 * Called when a USB CDC connection is established. Resets the controller
 * state and starts sending initialization requests to the host.
 */
void controller_connected();

/**
 * @brief Handle controller disconnection
 *
 * Called when the USB CDC connection is lost. Stops servo operation and
 * returns the controller to the idle state.
 */
void controller_disconnected();

/**
 * @brief Timer callback to send initialization requests
 *
 * Periodically sends INIT messages to the host to request configuration data.
 *
 * @param xTimer The timer handle that triggered this callback
 */
void send_init_request(TimerHandle_t xTimer);

/**
 * @brief Timer callback to check for reset requests
 *
 * Checks if the CONTROLLER_RESET_PIN is high, indicating a request to reset
 * the controller configuration.
 *
 * @param xTimer The timer handle that triggered this callback
 */
void controller_reset_request_check_timer_callback(TimerHandle_t xTimer);

/**
 * @brief Signal that configuration has been received
 *
 * Called when valid configuration data has been received from the host.
 * Transitions the controller to the running state.
 */
void firmware_configuration_received();

/**
 * @brief Signal whether a frame has been received from the controller
 *
 * Updates the first_frame_received state and controls the power relay.
 * When the first frame is received, the power relay is enabled.
 *
 * @param yesOrNo true if a frame has been received, false otherwise
 */
void first_frame_received(bool yesOrNo);

/**
 * @brief Check for controller reset requests
 *
 * Legacy function now replaced by
 * controller_reset_request_check_timer_callback(). Kept for compatibility.
 */
void check_for_controller_reset_request();

/**
 * @brief Check if a motor is configured by the computer
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if motor is configured, false if not (or motor not found)
 */
bool is_motor_configured(const char *motor_id);

/**
 * @brief Check if all motors are configured
 *
 * @return true if all motors have been configured by the computer
 */
bool are_all_motors_configured(void);

#ifdef CC_VER4

#include "dynamixel/dynamixel_hal.h"
#include "dynamixel/dynamixel_servo.h"

/**
 * @brief Structure mapping a Dynamixel motor to its configuration
 */
typedef struct {
    u8 dxl_id;              /**< Dynamixel bus address (1-253) */
    u32 min_position;       /**< Minimum allowed position (0-4095) */
    u32 max_position;       /**< Maximum allowed position (0-4095) */
    u32 requested_position; /**< Next position to write via Sync Write */
    bool is_configured;     /**< True if this motor has been configured */
} DynamixelMotorEntry;

/**
 * @brief Array of Dynamixel motor entries
 */
extern DynamixelMotorEntry dxl_motors[MAX_DYNAMIXEL_SERVOS];

/**
 * @brief Number of configured Dynamixel motors
 */
extern u8 dxl_motor_count;

/**
 * @brief Reset the servo motor map
 *
 * Clears configuration fields (min/max pulse, position, is_configured)
 * for all PWM servo entries so the map can be repopulated from a new
 * CONFIG message. Hardware pin assignments are preserved.
 */
void resetServoMotorMap(void);

/**
 * @brief Reset the Dynamixel motor map
 *
 * Clears all configured Dynamixel motors so the map can be
 * repopulated from a new CONFIG message.
 */
void resetDynamixelMotorMap(void);

/**
 * @brief Configure a Dynamixel servo
 *
 * Adds the servo to the motor map and sets its Profile Velocity.
 * Does NOT enable torque (torque tracks power relay state).
 *
 * @param dxl_id Bus address (1-253)
 * @param min_pos Minimum position in Dynamixel units (0-4095)
 * @param max_pos Maximum position in Dynamixel units (0-4095)
 * @param profile_velocity Profile velocity for motion smoothing
 * @return true if configuration was successful
 */
bool configureDynamixelServo(u8 dxl_id, u32 min_pos, u32 max_pos, u32 profile_velocity);

/**
 * @brief Request a Dynamixel servo move to a position
 *
 * @param dxl_id Bus address (1-253)
 * @param position Position in Dynamixel units (0-4095)
 * @return true if the position was accepted
 */
bool requestDynamixelPosition(u8 dxl_id, u32 position);

/**
 * @brief Enable or disable torque on all configured Dynamixel servos
 *
 * Called from first_frame_received(true) to enable, and from
 * controller_disconnected() / emergency stop to disable.
 *
 * @param enable true to enable torque, false to disable
 */
void dynamixel_set_torque_all(bool enable);

/**
 * @brief FreeRTOS task for Dynamixel servo control
 *
 * Runs at 50Hz: Sync Write positions every frame, Sync Read
 * telemetry every DXL_SENSOR_REPORT_INTERVAL_FRAMES frames.
 */
portTASK_FUNCTION_PROTO(dynamixel_controller_task, pvParameters);

/**
 * @brief Get the Dynamixel bus metrics for stats reporting
 *
 * @return Pointer to the metrics struct, or NULL if HAL not initialized
 */
const dxl_metrics_t *controller_get_dxl_metrics(void);

#endif
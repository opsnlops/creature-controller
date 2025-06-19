

#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "controller/controller.h"
#include "device/power_control.h"
#include "logging/logging.h"


#include "controller/config.h"
#include "types.h"


#ifdef CC_VER3

// Defined in controller/controller.c
extern MotorMap motor_map[MOTOR_MAP_SIZE];


/**
 * @brief Initialize power control pins for all motors
 *
 * Sets up GPIO pins for motor power control as outputs and ensures
 * all motors start in the powered-off state for safety.
 */
void init_motor_power_control(void) {
    info("initializing motor power control pins");

    for (size_t i = 0; i < MOTOR_MAP_SIZE; i++) {
        gpio_init(motor_map[i].power_pin);
        gpio_set_dir(motor_map[i].power_pin, GPIO_OUT);

        // Start with motors powered off for safety
        gpio_put(motor_map[i].power_pin, false);

        debug("motor %s power pin %u initialized (powered off)",
              motor_map[i].motor_id, motor_map[i].power_pin);
    }

    info("all motor power control pins initialized");
}

/**
 * @brief Enable power to a specific motor
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if power was enabled successfully, false if motor not found
 */
bool enable_motor_power(const char* motor_id) {
    if (motor_id == NULL || motor_id[0] == '\0') {
        warning("motor_id is null while enabling motor power");
        return false;
    }

    u8 motor_index = getMotorMapIndex(motor_id);
    if (motor_index == INVALID_MOTOR_ID) {
        warning("invalid motor ID while enabling power: %s", motor_id);
        return false;
    }

    bool result = false;

    // Take the mutex to ensure thread-safe access
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        gpio_put(motor_map[motor_index].power_pin, true);
        result = true;

        info("enabled power for motor %s (pin %u)",
             motor_id, motor_map[motor_index].power_pin);

        xSemaphoreGive(motor_map_mutex);
    } else {
        warning("failed to take motor_map_mutex in enable_motor_power");
    }

    return result;
}

/**
 * @brief Disable power to a specific motor
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if power was disabled successfully, false if motor not found
 */
bool disable_motor_power(const char* motor_id) {
    if (motor_id == NULL || motor_id[0] == '\0') {
        warning("motor_id is null while disabling motor power");
        return false;
    }

    u8 motor_index = getMotorMapIndex(motor_id);
    if (motor_index == INVALID_MOTOR_ID) {
        warning("invalid motor ID while disabling power: %s", motor_id);
        return false;
    }

    bool result = false;

    // Take the mutex to ensure thread-safe access
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        gpio_put(motor_map[motor_index].power_pin, false);
        result = true;

        info("disabled power for motor %s (pin %u)",
             motor_id, motor_map[motor_index].power_pin);

        xSemaphoreGive(motor_map_mutex);
    } else {
        warning("failed to take motor_map_mutex in disable_motor_power");
    }

    return result;
}

/**
 * @brief Enable power to all configured motors
 *
 * Only enables power to motors that have been properly configured by the computer.
 * Unconfigured motors remain safely powered off.
 *
 * @return true if all configured motors were powered successfully
 */
bool enable_all_motors(void) {
    bool all_successful = true;
    u8 configured_count = 0;
    u8 powered_count = 0;

    info("enabling power to all configured motors - time to get this bunny show hopping!");

    // Take the mutex to ensure thread-safe access during the whole operation
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < MOTOR_MAP_SIZE; i++) {
            if (motor_map[i].is_configured) {
                configured_count++;

                // Release mutex temporarily for the enable_motor_power call
                // (which takes its own mutex)
                xSemaphoreGive(motor_map_mutex);

                if (enable_motor_power(motor_map[i].motor_id)) {
                    powered_count++;
                    debug("powered up configured motor %s", motor_map[i].motor_id);
                } else {
                    error("failed to enable power for configured motor %s", motor_map[i].motor_id);
                    all_successful = false;
                }

                // Re-take mutex for next iteration
                if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) != pdTRUE) {
                    error("failed to re-take motor_map_mutex in enable_all_motors");
                    return false;
                }
            } else {
                debug("skipping unconfigured motor %s", motor_map[i].motor_id);
            }
        }
        xSemaphoreGive(motor_map_mutex);
    } else {
        error("failed to take motor_map_mutex in enable_all_motors");
        return false;
    }

    if (configured_count == 0) {
        warning("no motors are configured yet");
        return false;
    }

    if (all_successful) {
        info("powered up %u/%u configured motors", powered_count, configured_count);
    } else {
        warning("only powered up %u/%u configured motors",
                powered_count, configured_count);
    }

    return all_successful;
}

/**
 * @brief Disable power to all motors (emergency stop)
 *
 * @return true if all motors were powered down successfully
 */
bool disable_all_motors(void) {
    bool all_successful = true;

    warning("disabling power to all motors - emergency bunny brake!");

    for (size_t i = 0; i < MOTOR_MAP_SIZE; i++) {
        if (!disable_motor_power(motor_map[i].motor_id)) {
            error("failed to disable power for motor %s", motor_map[i].motor_id);
            all_successful = false;
        }
    }

    if (all_successful) {
        info("all motors safely powered down - bunny is now stationary! 🐰");
    } else {
        error("some motors failed to power down - that's a hare-raising situation!");
    }

    return all_successful;
}

/**
 * @brief Check if a motor has power enabled
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if motor has power, false if not (or motor not found)
 */
bool is_motor_powered(const char* motor_id) {
    if (motor_id == NULL || motor_id[0] == '\0') {
        warning("motor_id is null while checking motor power");
        return false;
    }

    u8 motor_index = getMotorMapIndex(motor_id);
    if (motor_index == INVALID_MOTOR_ID) {
        warning("invalid motor ID while checking power: %s", motor_id);
        return false;
    }

    bool powered = false;

    // Take the mutex to ensure thread-safe access
    if (xSemaphoreTake(motor_map_mutex, portMAX_DELAY) == pdTRUE) {
        powered = gpio_get(motor_map[motor_index].power_pin);
        xSemaphoreGive(motor_map_mutex);
    } else {
        warning("failed to take motor_map_mutex in is_motor_powered");
    }

    return powered;
}

#endif

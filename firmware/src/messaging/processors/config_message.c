
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>

#include <timers.h>

#include "controller/controller.h"
#include "logging/logging.h"
#include "messaging/messaging.h"
#include "util/string_utils.h"

#include "types.h"

extern volatile bool controller_safe_to_run;
extern TimerHandle_t controller_init_request_timer;
extern enum FirmwareState controller_firmware_state;

/*
 * Example message:
 *
 * SERVO B0 1475 1950
 */

bool handleConfigMessage(const GenericMessage *msg) {

    debug("received a config message from the controller");

    // Stop the init timer
    xTimerStop(controller_init_request_timer, 0);

    debug("handling config message");

    for (int i = 0; i < msg->tokenCount; ++i) {
        const char *orig_token = msg->tokens[i];

        // Make a copy of the token for strtok_r to modify
        char token_copy[MAX_TOKEN_LENGTH];
        strcpy(token_copy, orig_token);

        char *temp_token = token_copy;
        char *motor_type = strtok_r(temp_token, " ", &temp_token);
        if (motor_type != NULL) {
            debug("motor type: %s", motor_type);
        }

        if (strcmp(motor_type, "SERVO") == 0) {
            // PWM servo path
            char *output_position = strtok_r(NULL, " ", &temp_token);
            if (output_position == NULL) {
                error("no output position specified for SERVO");
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            verbose("output_position: %s", output_position);

            char *min_us_str = strtok_r(NULL, " ", &temp_token);
            if (min_us_str == NULL) {
                error("no min_us specified for SERVO %s", output_position);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            const u16 min_us = stringToU16(min_us_str);
            if (min_us == 0) {
                error("min_us can't be parsed to u16: %s", min_us_str);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            verbose("min_us: %u", min_us);

            const char *max_us_str = strtok_r(NULL, " ", &temp_token);
            if (max_us_str == NULL) {
                error("no max_us specified for SERVO %s", output_position);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            const u16 max_us = stringToU16(max_us_str);
            if (max_us == 0) {
                error("max_us can't be parsed to u16: %s", max_us_str);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            verbose("max_us: %u", max_us);

            if (!configureServoMinMax(output_position, min_us, max_us)) {
                error("unable to configure servo: %s", output_position);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
        }
#ifdef CC_VER4
        else if (strcmp(motor_type, "DYNAMIXEL") == 0) {
            // Dynamixel servo path: DYNAMIXEL <dxl_id> <min_pos> <max_pos> <profile_velocity>
            char *dxl_id_str = strtok_r(NULL, " ", &temp_token);
            if (dxl_id_str == NULL) {
                error("no dxl_id specified for DYNAMIXEL");
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            const u8 dxl_id = (u8)stringToU16(dxl_id_str);

            char *min_pos_str = strtok_r(NULL, " ", &temp_token);
            if (min_pos_str == NULL) {
                error("no min_position specified for DYNAMIXEL %u", dxl_id);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            const u32 min_pos = (u32)stringToU16(min_pos_str);

            char *max_pos_str = strtok_r(NULL, " ", &temp_token);
            if (max_pos_str == NULL) {
                error("no max_position specified for DYNAMIXEL %u", dxl_id);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            const u32 max_pos = (u32)stringToU16(max_pos_str);

            char *velocity_str = strtok_r(NULL, " ", &temp_token);
            if (velocity_str == NULL) {
                error("no profile_velocity specified for DYNAMIXEL %u", dxl_id);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
            const u32 profile_velocity = (u32)stringToU16(velocity_str);

            if (!configureDynamixelServo(dxl_id, min_pos, max_pos, profile_velocity)) {
                error("unable to configure Dynamixel servo: %u", dxl_id);
                xTimerStart(controller_init_request_timer, 0);
                return false;
            }
        }
#endif
        else {
            error("unknown motor type: %s", motor_type);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }
    }

    // Let the controller know we're good to go! 🎉
    firmware_configuration_received();

    return true;
}

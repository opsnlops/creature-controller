
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

        // We only know how to handle servos at this point
        if (strcmp(motor_type, "SERVO") != 0) {
            error("unknown motor type: %s", motor_type);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }

        char *output_position = strtok_r(NULL, " ", &temp_token);
        if (output_position == NULL) {
            error("no output position specified: %s", output_position);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }
        verbose("output_position: %s", output_position);

        char *min_us_str = strtok_r(NULL, " ", &temp_token);
        if (min_us_str == NULL) {
            error("no min_us_str specified: %s", output_position);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }
        const u16 min_us = stringToU16(min_us_str);
        if (min_us == 0) {
            error("min_us_str can't be parsed to u16: %s", min_us_str);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }
        verbose("min_us: %u", min_us);

        const char *max_us_str = strtok_r(NULL, " ", &temp_token);
        if (max_us_str == NULL) {
            error("no max_us_str specified: %s", output_position);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }
        const u16 max_us = stringToU16(max_us_str);
        if (max_us == 0) {
            error("max_us_str can't be parsed to u16: %s", min_us_str);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }
        verbose("max_us: %u", max_us);


        // Ask the controller to configure the servo
        if(!configureServoMinMax(output_position, min_us, max_us)) {
            error("Unable to configure servo: %s", output_position);
            xTimerStart(controller_init_request_timer, 0);
            return false;
        }

    }

    // Let the controller know we're good to go! 🎉
    firmware_configuration_received();

    return true;

}

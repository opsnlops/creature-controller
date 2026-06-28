
#include <string.h>

#include "controller/controller.h"
#include "logging/logging.h"
#include "messaging/messaging.h"
#include "util/string_utils.h"

#ifdef CC_VER4
#include "dynamixel/dynamixel_registers.h"
#endif

#include "types.h"

extern volatile u64 position_messages_processed;
extern volatile bool controller_safe_to_run;
extern volatile bool has_first_frame_been_received;

bool handlePositionMessage(const GenericMessage *msg) {

    verbose("handling position message");

    // The safety gate is checked once up front; dropping for safety is not a
    // malformed-message condition, so the frame is "handled" without dispatch.
    if (!controller_safe_to_run) {
        warning("dropping position message because we haven't been told it's safe");
        position_messages_processed = position_messages_processed + 1;
        return true;
    }

    for (int i = 0; i < msg->tokenCount; ++i) {
        const char *orig_token = msg->tokens[i];

        // Make a copy of the token for strtok_r to modify
        char token_copy[MAX_TOKEN_LENGTH];
        strcpy(token_copy, orig_token);

        char *temp_token = token_copy;
        char *position = strtok_r(temp_token, " ", &temp_token);
        char *value = strtok_r(NULL, " ", &temp_token);

        if (position == NULL || position[0] == '\0') {
            warning("rejecting POS frame: token %d has no motor identifier", i);
            return false;
        }
        if (value == NULL || value[0] == '\0') {
            warning("rejecting POS frame: token %d (%s) has no value", i, position);
            return false;
        }

        verbose("incoming position message: %s %s", position, value);

#ifdef CC_VER4
        if (position[0] == 'D') {
            // Dynamixel: D<id> <position>
            if (position[1] == '\0') {
                warning("rejecting POS frame: Dynamixel token %d has no ID after 'D'", i);
                return false;
            }
            const u16 dxl_id_raw = stringToU16(&position[1]);
            if (dxl_id_raw == 0 || dxl_id_raw > DXL_MAX_ID) {
                warning("rejecting POS frame: Dynamixel ID %u out of range [1-%u]", dxl_id_raw, DXL_MAX_ID);
                return false;
            }
            const u32 dxl_pos = (u32)stringToU16(value);
            if (dxl_pos > DXL_POSITION_MAX) {
                warning("rejecting POS frame: Dynamixel %u position %lu out of range [0-%u]", dxl_id_raw,
                        (unsigned long)dxl_pos, DXL_POSITION_MAX);
                return false;
            }
            if (!requestDynamixelPosition((u8)dxl_id_raw, dxl_pos)) {
                warning("rejecting POS frame: requestDynamixelPosition(%u, %lu) failed", dxl_id_raw,
                        (unsigned long)dxl_pos);
                return false;
            }
        } else {
            // PWM: <pin> <microseconds>
            if (!requestServoPosition(position, stringToU16(value))) {
                warning("rejecting POS frame: requestServoPosition(%s, %s) failed", position, value);
                return false;
            }
        }
#else
        if (!requestServoPosition(position, stringToU16(value))) {
            warning("rejecting POS frame: requestServoPosition(%s, %s) failed", position, value);
            return false;
        }
#endif
    }

    // Was this the first frame we've received? Only signal once we know the
    // whole frame was accepted.
    if (!has_first_frame_been_received) {
        first_frame_received(true);
    }

    position_messages_processed = position_messages_processed + 1;

    return true;
}

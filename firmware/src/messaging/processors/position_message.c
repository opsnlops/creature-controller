
#include <stddef.h>
#include <string.h>

#include "controller/controller.h"
#include "logging/logging.h"
#include "messaging/messaging.h"
#include "util/string_utils.h"

#include "controller-config.h"

volatile u64 position_messages_processed = 0UL;
extern volatile bool controller_safe_to_run;
extern volatile bool has_first_frame_been_received;

bool handlePositionMessage(const GenericMessage *msg) {

    verbose("handling position message");

    for (int i = 0; i < msg->tokenCount; ++i) {
        const char *orig_token = msg->tokens[i];

        // Make a copy of the token for strtok_r to modify
        char token_copy[MAX_TOKEN_LENGTH];
        strcpy(token_copy, orig_token);

        char *temp_token = token_copy;
        char *position = strtok_r(temp_token, " ", &temp_token);
        if (position != NULL) {
            verbose("First part: %s", position);
        }

        char *value = strtok_r(NULL, " ", &temp_token);
        if (value != NULL) {
            verbose("Second part: %s", value);
        }

        verbose("incoming position message: %s %s", position, value);

        if(!controller_safe_to_run) {
            warning("dropping position message because we haven't been told it's safe");
        }
        else {
            requestServoPosition(position, stringToU16(value));

            // Was this the first frame we've received?
            if(!has_first_frame_been_received) {
                first_frame_received(true);
            }
        }
    }

    position_messages_processed = position_messages_processed + 1;

    return true;


}


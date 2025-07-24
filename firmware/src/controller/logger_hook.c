
#include <stdio.h>

#include "logging/logging_api.h"

#include "io/message_processor.h"

#include "config.h"
#include "types.h"

/**
 * Called at the end of the logging process. Used to allow
 * for a hook to be called after logging has been completed.
 *
 * @param message the message that was logged
 * @param message_length the length of the message that was logged
 */
void acw_post_logging_hook(char *message, uint8_t message_length) {

    (void)message_length;

    // Send the message to the controller
    send_to_controller(message);

    // If we have a console, send the message to the console
#if LOGGING_LOG_VIA_PRINTF
    printf("%s\n", message);
#endif
}


#pragma once

#include <FreeRTOS.h>
#include <task.h>


/**
 * Called at the end of the logging process. Used to allow
 * for a hook to be called after logging has been completed.
 *
 * @param message the message that was logged
 * @param message_length the length of the message that was logged
 */
void acw_post_logging_hook(char *message, uint8_t message_length);

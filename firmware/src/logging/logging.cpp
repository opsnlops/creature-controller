
#include <cstdio>
#include <cstdarg>
#include <cstring>

#include <FreeRTOS.h>
#include <queue.h>
#include "pico/time.h"

#include "tasks.h"
#include "logging.h"


extern TaskHandle_t log_queue_reader_task_handle;   // in main.cpp

QueueHandle_t creature_log_message_queue_handle;


bool logging_queue_exists = false;
extern bool volatile log_to_shell;


void logger_init() {
    creature_log_message_queue_handle = xQueueCreate(LOGGING_QUEUE_LENGTH, sizeof(LogMessage));
    vQueueAddToRegistry(creature_log_message_queue_handle, "log_message_queue");
    logging_queue_exists = true;
    start_log_reader();
}

bool inline _is_safe_to_log() {
    return (logging_queue_exists && !xQueueIsQueueFullFromISR(creature_log_message_queue_handle));
}

void __unused verbose(const char *message, ...) {
#if LOGGING_LEVEL > 4

    // If the logging queue if full, stop now
    if(!_is_safe_to_log())
        return;

    // Copy the arguments to a new va_list
    va_list args;
    va_start(args, message);

    struct LogMessage lm = createMessageObject(LOG_LEVEL_VERBOSE, message, args);
    va_end(args);

    xQueueSendToBackFromISR(creature_log_message_queue_handle, &lm, nullptr);
#endif
}

void debug(const char *message, ...) {
#if LOGGING_LEVEL > 3

    // If the logging queue if full, stop now
    if(!_is_safe_to_log())
        return;

    // Copy the arguments to a new va_list
    va_list args;
    va_start(args, message);

    struct LogMessage lm = createMessageObject(LOG_LEVEL_DEBUG, message, args);
    va_end(args);

   xQueueSendToBackFromISR(creature_log_message_queue_handle, &lm, nullptr);

#endif
}

void info(const char *message, ...) {
#if LOGGING_LEVEL > 2

    // If the logging queue if full, stop now
    if(!_is_safe_to_log())
        return;

    // Copy the arguments to a new va_list
    va_list args;
    va_start(args, message);

    struct LogMessage lm = createMessageObject(LOG_LEVEL_INFO, message, args);
    va_end(args);

    xQueueSendToBackFromISR(creature_log_message_queue_handle, &lm, nullptr);

#endif
}

void warning(const char *message, ...) {
#if LOGGING_LEVEL > 1

    // If the logging queue if full, stop now
    if(!_is_safe_to_log())
        return;

    // Copy the arguments to a new va_list
    va_list args;
    va_start(args, message);

    struct LogMessage lm = createMessageObject(LOG_LEVEL_WARNING, message, args);
    va_end(args);

    xQueueSendToBackFromISR(creature_log_message_queue_handle, &lm, nullptr);

#endif
}

void error(const char *message, ...) {
#if LOGGING_LEVEL > 0

    // If the logging queue if full, stop now
    if(!_is_safe_to_log())
        return;

    // Copy the arguments to a new va_list
    va_list args;
    va_start(args, message);

    struct LogMessage lm = createMessageObject(LOG_LEVEL_ERROR, message, args);
    va_end(args);

    xQueueSendToBackFromISR(creature_log_message_queue_handle, &lm, nullptr);

#endif
}

void __unused fatal(const char *message, ...) {

    // If the logging queue if full, stop now
    if(!_is_safe_to_log())
        return;

    // Copy the arguments to a new va_list
    va_list args;
    va_start(args, message);

    struct LogMessage lm = createMessageObject(LOG_LEVEL_FATAL, message, args);
    va_end(args);

    xQueueSendToBackFromISR(creature_log_message_queue_handle, &lm, nullptr);

}

struct LogMessage createMessageObject(uint8_t level, const char *message, va_list args) {
    char buffer[LOGGING_MESSAGE_MAX_LENGTH + 1];
    memset(buffer, '\0', LOGGING_MESSAGE_MAX_LENGTH + 1);

    vsnprintf(buffer, LOGGING_MESSAGE_MAX_LENGTH, message, args);

    LogMessage lm{};
    lm.level = level;
    memcpy(lm.message, buffer, LOGGING_MESSAGE_MAX_LENGTH);
    return lm;
}

void start_log_reader() {
    xTaskCreate(log_queue_reader_task,
                "log_queue_reader_task",
                1512,
                nullptr,
                1,
                &log_queue_reader_task_handle);
}

/**
 * @brief Creates a task that polls the logging queue
 *
 * It then spits things to the Serial port, and optionally to syslog so that a
 * Linux host can handle the heavy lifting.
 */
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

portTASK_FUNCTION(log_queue_reader_task, pvParameters) {

    LogMessage lm{};
    char levelBuffer[4];
    memset(&levelBuffer, '\0', 4);

    for (EVER) {
        if (xQueueReceive(creature_log_message_queue_handle, &lm, (TickType_t) portMAX_DELAY) == pdPASS) {
            switch (lm.level) {
                case LOG_LEVEL_VERBOSE:
                    strncpy(levelBuffer, "[V] ", 3);
                    break;
                case LOG_LEVEL_DEBUG:
                    strncpy(levelBuffer, "[D] ", 3);
                    break;
                case LOG_LEVEL_INFO:
                    strncpy(levelBuffer, "[I] ", 3);
                    break;
                case LOG_LEVEL_WARNING:
                    strncpy(levelBuffer, "[W] ", 3);
                    break;
                case LOG_LEVEL_ERROR:
                    strncpy(levelBuffer, "[E] ", 3);
                    break;
                case LOG_LEVEL_FATAL:
                    strncpy(levelBuffer, "[F] ", 3);
                    break;
                default:
                    strncpy(levelBuffer, "[?] ", 3);
            }

            // Format our message
            uint32_t time = to_ms_since_boot(get_absolute_time());
            printf("[%lu]%s %s\n", time, levelBuffer, lm.message);

#if 0
            if(log_to_shell) {
                auto message = (char*)pvPortMalloc(strlen(lm.message) + 33);
                memset(message, '\0', strlen(lm.message) + 33);
                snprintf(message, strlen(lm.message) + 32, "[%lu]%s %s\n\r", time, levelBuffer, lm.message);
                print_log_to_shell(message);
                vPortFree(message);
            }
#endif

            // Wipe the buffer for next time
            memset(&levelBuffer, '\0', 4);

        }
    }
}

#pragma clang diagnostic pop

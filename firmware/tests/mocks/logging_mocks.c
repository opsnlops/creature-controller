/**
 * @file logging_mocks.c
 * @brief Mock implementations of logging functions
 */

#include "logging_mocks.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

// Buffer to capture log messages for verification
static char log_buffer[MAX_LOG_ENTRIES][MAX_LOG_LENGTH];
static u16 log_count = 0;
static u8 current_log_level = LOG_LEVEL_INFO; // Default log level

void reset_log_mocks(void) {
    printf("[TEST DEBUG] Resetting log mocks\n");
    memset(log_buffer, 0, sizeof(log_buffer));
    log_count = 0;
}

void set_log_level(u8 level) {
    printf("[TEST DEBUG] Setting log level to %u\n", level);
    current_log_level = level;
}

void log_message(u8 level, const char *format, ...) {
    // Verify inputs
    if (format == NULL) {
        printf("[TEST DEBUG] log_message called with NULL format\n");
        return;
    }

    // Only log if level is less than or equal to current level
    if (level <= current_log_level && log_count < MAX_LOG_ENTRIES) {
        va_list args;
        va_start(args, format);

        // Format the log prefix
        char prefix[10] = {0};
        switch (level) {
            case LOG_LEVEL_VERBOSE:
                strcpy(prefix, "[V] ");
                break;
            case LOG_LEVEL_DEBUG:
                strcpy(prefix, "[D] ");
                break;
            case LOG_LEVEL_INFO:
                strcpy(prefix, "[I] ");
                break;
            case LOG_LEVEL_WARNING:
                strcpy(prefix, "[W] ");
                break;
            case LOG_LEVEL_ERROR:
                strcpy(prefix, "[E] ");
                break;
            case LOG_LEVEL_FATAL:
                strcpy(prefix, "[F] ");
                break;
            default:
                strcpy(prefix, "[?] ");
                break;
        }

        if (log_count >= MAX_LOG_ENTRIES) {
            printf("[TEST DEBUG] Log buffer full - discarding message\n");
            va_end(args);
            return;
        }

        // Copy prefix to log buffer
        strcpy(log_buffer[log_count], prefix);

        // Format message
        vsnprintf(log_buffer[log_count] + strlen(prefix),
                  MAX_LOG_LENGTH - strlen(prefix),
                  format, args);

        va_end(args);
        log_count++;

        // Also print to stdout for debugging
        printf("[MOCK LOG] %s\n", log_buffer[log_count-1]);
    }
}

// Implement the log functions from logging.h
void verbose_mock(const char *message, ...) {
    if (message == NULL) {
        printf("[TEST DEBUG] verbose_mock called with NULL message\n");
        return;
    }

    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH] = {0};
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_VERBOSE, buffer);
    va_end(args);
}

void debug_mock(const char *message, ...) {
    if (message == NULL) {
        printf("[TEST DEBUG] debug_mock called with NULL message\n");
        return;
    }

    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH] = {0};
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_DEBUG, buffer);
    va_end(args);
}

void info_mock(const char *message, ...) {
    if (message == NULL) {
        printf("[TEST DEBUG] info_mock called with NULL message\n");
        return;
    }

    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH] = {0};
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_INFO, buffer);
    va_end(args);
}

void warning_mock(const char *message, ...) {
    if (message == NULL) {
        printf("[TEST DEBUG] warning_mock called with NULL message\n");
        return;
    }

    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH] = {0};
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_WARNING, buffer);
    va_end(args);
}

void error_mock(const char *message, ...) {
    if (message == NULL) {
        printf("[TEST DEBUG] error_mock called with NULL message\n");
        return;
    }

    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH] = {0};
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_ERROR, buffer);
    va_end(args);
}

void fatal_mock(const char *message, ...) {
    if (message == NULL) {
        printf("[TEST DEBUG] fatal_mock called with NULL message\n");
        return;
    }

    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH] = {0};
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_FATAL, buffer);
    va_end(args);
}

// Functions to help test verification
const char* get_log_message(u16 index) {
    if (index < log_count) {
        return log_buffer[index];
    }
    return NULL;
}

u16 get_log_count(void) {
    return log_count;
}

bool log_contains(const char* substring) {
    if (substring == NULL) {
        printf("[TEST DEBUG] log_contains called with NULL substring\n");
        return false;
    }

    for (u16 i = 0; i < log_count; i++) {
        if (strstr(log_buffer[i], substring) != NULL) {
            return true;
        }
    }
    return false;
}

// Add missing functions required by the firmware
bool is_safe_to_log(void) {
    // Always safe to log in tests
    return true;
}

// Additional acw_post_logging_hook for firmware_logger_hook.c
void acw_post_logging_hook(char *message, u8 message_length) {
    if (message == NULL) {
        printf("[TEST DEBUG] acw_post_logging_hook called with NULL message\n");
        return;
    }

    // Just print in tests
    printf("[MOCK HOOK] %.*s\n", message_length, message);
}
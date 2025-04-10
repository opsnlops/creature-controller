/**
 * @file logging_mocks.c
 * @brief Mock implementations of logging functions
 */

#include "logging_mocks.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Buffer to capture log messages for verification
static char log_buffer[MAX_LOG_ENTRIES][MAX_LOG_LENGTH];
static u16 log_count = 0;
static u8 current_log_level = LOG_LEVEL_INFO; // Default log level

void reset_log_mocks(void) {
    memset(log_buffer, 0, sizeof(log_buffer));
    log_count = 0;
}

void set_log_level(u8 level) {
    current_log_level = level;
}

void log_message(u8 level, const char *format, ...) {
    // Only log if level is less than or equal to current level
    if (level <= current_log_level && log_count < MAX_LOG_ENTRIES) {
        va_list args;
        va_start(args, format);

        // Format the log prefix
        char prefix[10];
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

        // Copy prefix to log buffer
        strcpy(log_buffer[log_count], prefix);

        // Format message
        vsnprintf(log_buffer[log_count] + strlen(prefix),
                  MAX_LOG_LENGTH - strlen(prefix),
                  format, args);

        va_end(args);
        log_count++;

        // Also print to stdout for debugging
        printf("%s\n", log_buffer[log_count-1]);
    }
}

// Implement the log functions from logging.h
void verbose_mock(const char *message, ...) {
    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH];
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_VERBOSE, buffer);
    va_end(args);
}

void debug_mock(const char *message, ...) {
    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH];
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_DEBUG, buffer);
    va_end(args);
}

void info_mock(const char *message, ...) {
    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH];
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_INFO, buffer);
    va_end(args);
}

void warning_mock(const char *message, ...) {
    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH];
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_WARNING, buffer);
    va_end(args);
}

void error_mock(const char *message, ...) {
    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH];
    vsnprintf(buffer, MAX_LOG_LENGTH, message, args);
    log_message(LOG_LEVEL_ERROR, buffer);
    va_end(args);
}

void fatal_mock(const char *message, ...) {
    va_list args;
    va_start(args, message);
    char buffer[MAX_LOG_LENGTH];
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
    for (u16 i = 0; i < log_count; i++) {
        if (strstr(log_buffer[i], substring) != NULL) {
            return true;
        }
    }
    return false;
}


/**
 * @file logging/logging.h
 * @brief Mock logging header that intercepts #include "logging/logging.h"
 *
 * This file lives at mocks/logging/logging.h so that firmware source files
 * (like messaging.c) pick it up instead of the real src/logging/logging.h.
 * The real header pulls in FreeRTOS and declares queue-based logging
 * infrastructure that doesn't exist in the test environment.
 *
 * Actual stub implementations are in logging_stubs.c.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "types.h"

// Logging level definitions (must match the real header)
#define LOG_LEVEL_VERBOSE 5
#define LOG_LEVEL_DEBUG 4
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_FATAL 0

void verbose(const char *message, ...);
void debug(const char *message, ...);
void info(const char *message, ...);
void warning(const char *message, ...);
void error(const char *message, ...);
void fatal(const char *message, ...);

bool is_safe_to_log(void);

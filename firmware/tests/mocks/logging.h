/**
 * @file logging.h
 * @brief Mock logging header for test framework
 */

#pragma once

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "types.h"

// Logging level definitions
#define LOG_LEVEL_VERBOSE 5
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_FATAL   0

// Mock functions defined in logging_mocks.c
void verbose(const char *message, ...);
void debug(const char *message, ...);
void info(const char *message, ...);
void warning(const char *message, ...);
void error(const char *message, ...);
void fatal(const char *message, ...);

// Stub functions for test mocks
bool is_safe_to_log(void);
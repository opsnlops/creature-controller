/**
 * @file logging_mocks.h
 * @brief Mock implementations of logging functions
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "stubs.h"

// Define logging levels
#define LOG_LEVEL_VERBOSE 5
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_FATAL   0

// Buffer settings for log capture
#define MAX_LOG_ENTRIES 100
#define MAX_LOG_LENGTH  256

// Mock functions for logging.h
void verbose_mock(const char *message, ...);
void debug_mock(const char *message, ...);
void info_mock(const char *message, ...);
void warning_mock(const char *message, ...);
void error_mock(const char *message, ...);
void fatal_mock(const char *message, ...);

// Implement is_safe_to_log for mocks
bool is_safe_to_log(void);

// Utility functions for test verification
void reset_log_mocks(void);
void set_log_level(u8 level);
const char* get_log_message(u16 index);
u16 get_log_count(void);
bool log_contains(const char* substring);

// Redefine logging functions to use mocks
#define verbose verbose_mock
#define debug debug_mock
#define info info_mock
#define warning warning_mock
#define error error_mock
#define fatal fatal_mock
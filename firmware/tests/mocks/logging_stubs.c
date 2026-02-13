/**
 * @file logging_stubs.c
 * @brief Stub implementations of logging functions for test binaries
 *
 * These provide real-symbol implementations of verbose(), debug(), etc.
 * so that firmware source files (like messaging.c) that call them directly
 * don't segfault at runtime.
 *
 * Note: Do NOT include logging_mocks.h here — it #defines verbose to
 * verbose_mock, which would rename these functions and defeat the purpose.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

void verbose(const char *message, ...) { (void)message; }

void debug(const char *message, ...) { (void)message; }

void info(const char *message, ...) { (void)message; }

void warning(const char *message, ...) { (void)message; }

void error(const char *message, ...) { (void)message; }

void fatal(const char *message, ...) { (void)message; }

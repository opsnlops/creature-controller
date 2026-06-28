#pragma once

/**
 * @file logging_config.h
 * @brief Compile-time configuration for the logging subsystem
 *
 * These were historically defined in the controller's config.h, which the
 * logging headers pulled in wholesale — leaking controller-specific pin and
 * buffer definitions into every firmware target. The logging system only ever
 * needed these four values, so they live here as the single source of truth.
 *
 * DEFAULT_LOGGING_LEVEL references the LOG_LEVEL_* macros from logging.h. That
 * header includes this one, so the names resolve at point of use.
 */

// How verbose the logger is until told otherwise.
#define DEFAULT_LOGGING_LEVEL LOG_LEVEL_DEBUG

// Depth of the queue that decouples log producers from the reader task.
#define LOGGING_QUEUE_LENGTH 100

// Maximum length of a single log message (sizes the LogMessage struct).
#define LOGGING_MESSAGE_MAX_LENGTH 256

// Also emit each message via printf(). Useful when a debugger is attached.
#define LOGGING_LOG_VIA_PRINTF 1

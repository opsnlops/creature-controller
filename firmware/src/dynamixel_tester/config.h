
#pragma once

/**
 * @file config.h
 * @brief Configuration for the Dynamixel Servo Tester
 */

// Dynamixel data line - any GPIO works with PIO
#define DXL_DATA_PIN 15

// Which PIO instance to use (pio0 has all 4 SMs free)
#define DXL_PIO_INSTANCE pio0

// Default baud rate for Dynamixel communication
#define DXL_DEFAULT_BAUD_RATE 1000000

// Shell command buffers
#define INCOMING_REQUEST_BUFFER_SIZE 128
#define OUTGOING_RESPONSE_BUFFER_SIZE 256

// Status LED pins
#define CDC_MOUNTED_LED_PIN 16
#define INCOMING_LED_PIN 17
#define OUTGOING_LED_PIN 18

// Watchdog configuration
#define WATCHDOG_TIMER_PERIOD_MS 1000
#define WATCHDOG_TIMEOUT_MS 5000

/*
 * Logging Config
 */
#define DEFAULT_LOGGING_LEVEL LOG_LEVEL_DEBUG
#define LOGGING_QUEUE_LENGTH 100
#define LOGGING_MESSAGE_MAX_LENGTH 256
#define LOGGING_LOG_VIA_PRINTF 1

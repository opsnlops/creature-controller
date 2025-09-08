#pragma once

#include <string.h>
#include <sys/cdefs.h>
#include <sys/select.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "tusb.h"

#include "types.h"

/**
 * @file usb.h
 * @brief USB subsystem interface for creature controller
 *
 * This module provides functions to initialize, start, and interact with the USB
 * subsystem. It handles USB device enumeration, CDC connection monitoring, and
 * message transmission over CDC.
 */

// USB configuration
#define BOARD_TUD_ROOT_HUB_PORT      0    // Root hub port for device mode

/**
 * @brief Initialize the USB subsystem
 *
 * Sets up the TinyUSB stack, initializes the device stack, and configures the
 * LED indicator. This must be called after the RTOS scheduler is started since
 * USB IRQ handlers use RTOS queue APIs.
 */
void usb_init(void);

/**
 * @brief Start USB service timers
 *
 * Creates and starts the FreeRTOS timers needed for USB operation:
 * - A 1ms timer for USB device tasks processing
 * - A 100ms timer for CDC connection monitoring
 *
 * This function should be called after usb_init().
 */
void usb_start(void);

/**
 * @brief Timer callback for USB device processing
 *
 * Called every 1ms to process TinyUSB tasks, ensuring the USB stack gets
 * regular processing time without blocking other operations.
 *
 * @param xTimer Timer handle that triggered this callback
 */
void usbDeviceTimerCallback(TimerHandle_t xTimer);

/**
 * @brief Timer callback for CDC connection monitoring
 *
 * Called every 100ms to check if the CDC connection state has changed.
 * Updates the LED indicator and notifies the controller module of
 * connection/disconnection events.
 *
 * @param xTimer Timer handle that triggered this callback
 */
void is_cdc_connected_timer(TimerHandle_t xTimer);

/**
 * @brief Send a message over CDC interface
 *
 * Transmits a null-terminated string over the CDC interface if connected.
 * If the device is not connected, the message is silently dropped and a
 * verbose log entry is generated.
 *
 * @param message Null-terminated string to transmit
 */
void cdc_send(const char *message);
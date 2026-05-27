#pragma once

#include <string.h>
#include <sys/cdefs.h>
#include <sys/select.h>

#include <FreeRTOS.h>
#include <task.h>

#include "tusb.h"

#include "types.h"

/**
 * @file usb.h
 * @brief USB subsystem interface for creature controller
 *
 * This module provides functions to initialize and start the USB subsystem.
 * A single task (usb_device_task, in usb.c) owns the entire TinyUSB device
 * stack: enumeration, CDC transmit, and connection monitoring. There is no
 * public "send" entry point - producers enqueue onto usb_serial_outgoing_messages
 * and the USB task drains it (TinyUSB OPT_OS_NONE requires a single context).
 */

// USB configuration
#define BOARD_TUD_ROOT_HUB_PORT 0 // Root hub port for device mode

/**
 * @brief Initialize the USB subsystem
 *
 * Sets up the TinyUSB stack, initializes the device stack, and configures the
 * LED indicator. This must be called after the RTOS scheduler is started since
 * USB IRQ handlers use RTOS queue APIs.
 */
void usb_init(void);

/**
 * @brief Start the single USB device task
 *
 * Creates usb_device_task, the sole owner of every tud_* call (device
 * servicing, CDC transmit, and connection monitoring). Call after usb_init().
 */
void usb_start(void);

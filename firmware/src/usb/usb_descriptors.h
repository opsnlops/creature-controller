#pragma once

#include <stdint.h>

#include "types.h"

/**
 * @file usb_descriptors.h
 * @brief USB descriptor configuration for creature controller
 *
 * Provides functions and structures for configuring USB device descriptors with
 * dynamically loaded values from EEPROM.
 */

// USB descriptor indices
#define USB_MANUFACTURER_INDEX      0x01
#define USB_PRODUCT_INDEX           0x02
#define USB_SERIAL_NUMBER_INDEX     0x03

// Global USB descriptor identifiers that can be customized
extern u16 usb_pid;
extern u16 usb_vid;
extern u16 usb_version;
extern char usb_serial[16];
extern char usb_product[16];
extern char usb_manufacturer[32];

/**
 * @brief Initialize USB descriptors with current values
 *
 * Call this function after loading EEPROM data and before initializing USB
 * to update the descriptor values with the currently set identifiers.
 */
void usb_descriptors_init(void);
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stddef.h>
#include <string.h>

#include "tusb.h"

#include "logging/logging.h"
#include "types.h"
#include "usb_descriptors.h"

// USB descriptor definitions
#define USB_BCD              0x0400   // 0x0400 is 4.0 in BCD format

// Endpoint definitions
#define EPNUM_CDC_NOTIF      0x83
#define EPNUM_CDC_OUT        0x02
#define EPNUM_CDC_IN         0x84

// Interface numbering
enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

// Total descriptors length
#define CONFIG_TOTAL_LEN     (TUD_CONFIG_DESC_LEN + (CFG_TUD_CDC * TUD_CDC_DESC_LEN))

// USB descriptor variables
u16 usb_pid = 0x1100;               // 0x1100 is "Creature Controller"
u16 usb_vid = 0x2E8A;               // 0x2E8A is the default VID for Pico projects
u16 usb_version = USB_BCD;
char usb_serial[16] = {0};
char usb_product[16] = {0};
char usb_manufacturer[32] = {0};

// Device descriptor - modified at runtime based on configuration
static tusb_desc_device_t usb_device_descriptor = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = USB_BCD,
        .bDeviceClass       = TUSB_CLASS_MISC,
        .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol    = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor           = 0,  // Will be filled in dynamically
        .idProduct          = 0,  // Will be filled in dynamically
        .bcdDevice          = 0,  // Will be filled in dynamically
        .iManufacturer      = USB_MANUFACTURER_INDEX,
        .iProduct           = USB_PRODUCT_INDEX,
        .iSerialNumber      = USB_SERIAL_NUMBER_INDEX,
        .bNumConfigurations = 0x01
};

// Configuration descriptor
u8 const desc_configuration[] = {
        // Config number, interface count, string index, total length, attribute, power in mA
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 200),

        // Main CDC interface
        TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

// Fallback string descriptors
char const *static_string_desc_arr[] = {
        (const char[]) {0x09, 0x04},    // 0: Supported language (English - 0x0409)
        "April's Creature Workshop",    // 1: Fallback Manufacturer
        "Unnamed Creature Controller",  // 2: Fallback Product
        "Unknown S/N",                  // 3: Fallback Serial
        "Creature Communications"       // 4: CDC 0 Description
};

/**
 * @brief Initialize USB descriptors with current configuration values
 *
 * This function updates the USB device descriptor structure with the current
 * vendor ID, product ID, and version number. It should be called after loading
 * any configuration values from EEPROM/flash and before initializing the USB stack.
 */
void usb_descriptors_init(void) {
    usb_device_descriptor.idVendor  = usb_vid;
    usb_device_descriptor.idProduct = usb_pid;
    usb_device_descriptor.bcdDevice = usb_version;
    debug("USB Descriptor initialized: VID=0x%04X, PID=0x%04X, Version=0x%04X",
          usb_vid, usb_pid, usb_version);
}

/**
 * @brief TinyUSB device descriptor callback
 *
 * This callback is invoked by the TinyUSB stack when the host requests
 * the device descriptor. It returns a pointer to our custom device descriptor
 * which has been previously initialized with the configured VID/PID values.
 *
 * @return Pointer to the USB device descriptor
 */
u8 const *tud_descriptor_device_cb(void) {
    debug("tud_descriptor_device_cb() called");
    return (u8 const *)&usb_device_descriptor;
}

/**
 * @brief TinyUSB configuration descriptor callback
 *
 * This callback is invoked by the TinyUSB stack when the host requests
 * the configuration descriptor. For this device, we only have a single
 * configuration that includes a CDC interface.
 *
 * @param index Configuration index (ignored as we only have one configuration)
 * @return Pointer to the USB configuration descriptor
 */
u8 const *tud_descriptor_configuration_cb(u8 index) {
    debug("tud_descriptor_configuration_cb: %d", index);
    return desc_configuration;
}

/**
 * @brief TinyUSB string descriptor callback
 *
 * This callback is invoked by the TinyUSB stack when the host requests
 * a string descriptor. It provides different strings based on the index:
 *
 * - Index 0: Language information (fixed)
 * - Index 1: Manufacturer name (from usb_manufacturer or fallback)
 * - Index 2: Product name (from usb_product or fallback)
 * - Index 3: Serial number (from usb_serial or fallback)
 * - Index 4+: Additional descriptors as defined in static_string_desc_arr
 *
 * The function converts ASCII strings to the UTF-16 format required by USB.
 *
 * @param index String descriptor index
 * @param langid Language ID (ignored in this implementation)
 * @return Pointer to the requested string descriptor in UTF-16 format
 */
u16 const *tud_descriptor_string_cb(u8 index, u16 langid) {
    (void)langid;
    static u16 _desc_str[32];
    u8 chr_count = 0;
    const char *str = NULL;

    switch(index) {
        case 0:
            // Language descriptor
            memcpy(&_desc_str[1], static_string_desc_arr[0], 2);
            chr_count = 1;
            break;
        case USB_MANUFACTURER_INDEX:
            // Use EEPROM value if available, otherwise fallback
            str = (usb_manufacturer[0] != '\0') ? usb_manufacturer : static_string_desc_arr[1];
            break;
        case USB_PRODUCT_INDEX:
            str = (usb_product[0] != '\0') ? usb_product : static_string_desc_arr[2];
            break;
        case USB_SERIAL_NUMBER_INDEX:
            str = (usb_serial[0] != '\0') ? usb_serial : static_string_desc_arr[3];
            break;
        default:
            // Use static strings for other indices
            if (index < sizeof(static_string_desc_arr) / sizeof(static_string_desc_arr[0])) {
                str = static_string_desc_arr[index];
            } else {
                return NULL;
            }
            break;
    }

    debug("tud_descriptor_string_cb: %d, %s", index, str);

    if (str) {
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (u8 i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }
    // First element: descriptor header with length and type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}
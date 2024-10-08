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

#include <FreeRTOS.h>

#include "tusb.h"

#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include "logging/logging.h"
#include "types.h"


#define USB_VID                     0x0666
#define USB_PID                     0x0010
#define USB_BCD                     0x0200

#define USB_MANUFACTURER_INDEX      0x01
#define USB_PRODUCT_INDEX           0x02
#define USB_SERIAL_NUMBER_INDEX     0x03


//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
        {
                .bLength            = sizeof(tusb_desc_device_t),
                .bDescriptorType    = TUSB_DESC_DEVICE,
                .bcdUSB             = USB_BCD,
                .bDeviceClass       = TUSB_CLASS_MISC,
                .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
                .bDeviceProtocol    = MISC_PROTOCOL_IAD,
                .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

                .idVendor           = USB_VID,
                .idProduct          = USB_PID,
                .bcdDevice          = 0x0201,               // This is the version number

                .iManufacturer      = USB_MANUFACTURER_INDEX,
                .iProduct           = USB_PRODUCT_INDEX,
                .iSerialNumber      = USB_SERIAL_NUMBER_INDEX,

                .bNumConfigurations = 0x01
        };

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
u8 const *tud_descriptor_device_cb(void) {

    debug("tud_descriptor_device_cb");

    return (u8 const *) &desc_device;
}


//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

// Total number of devices
#define  CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + (CFG_TUD_CDC * TUD_CDC_DESC_LEN))

#define EPNUM_CDC_NOTIF     0x83
#define EPNUM_CDC_OUT       0x02
#define EPNUM_CDC_IN        0x84

u8 const desc_configuration[] =
        {
                // Config number, interface count, string index, total length, attribute, power in mA
                TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 200),

                // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
                TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)

        };


// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
u8 const *tud_descriptor_configuration_cb(uint8_t index) {
    debug("tud_descriptor_configuration_cb: %d", index);

    // This example use the same configuration for both high and full speed mode
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const *string_desc_arr[] =
        {
                (const char[]) {0x09, 0x04}, // 0: is supported language is English (0x0409)
                "April's Creature Workshop",                     // 1: Manufacturer
                "Creature Controller",              // 2: Product
                NULL,                           // Figured out at runtime (serial number)
                "Debug Console"
        };

static u16 _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
u16 const *tud_descriptor_string_cb(u8 index, u16 langid) {
    (void) langid;

    u8 chr_count;

    // Null out the description string
    memset(_desc_str, '\0', 32);

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == USB_SERIAL_NUMBER_INDEX) {

        // Grab our unique_id
        pico_unique_board_id_t board_id;
        pico_get_unique_board_id(&board_id);
        char *pico_board_id = (char *) pvPortMalloc(sizeof(char) * (2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1));
        pico_get_unique_board_id_string(pico_board_id, 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1);

        chr_count = strlen(pico_board_id);

        // Shove this into the array
        for (u8 i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = pico_board_id[i];
        }

        vPortFree(pico_board_id);
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;

        const char *str = string_desc_arr[index];

        // Cap at max char
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 31) chr_count = 31;

        // Convert ASCII string into UTF-16
        for (u8 i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (u16) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}
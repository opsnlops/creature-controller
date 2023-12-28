
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "logging/logging.h"


#include "controller-config.h"


u16 stringToU16(const char *str) {

    // Make sure this is a valid string
    if (str == NULL) {
        return 0;
    }

    // Skip leading whitespace
    while (*str != '\0' && isspace((unsigned char) *str)) {
        str++;
    }

    // Check if the string is empty after skipping whitespace
    if (*str == '\0') {
        return 0;
    }

    // Detect hexadecimal (starts with "0x" or "0X")
    bool isHex = (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'));
    char *endPointer;
    errno = 0;

    // Convert to u16
    unsigned long value = isHex ? strtoul(str, &endPointer, 16) : strtoul(str, &endPointer, 10);

    // Check for conversion errors
    if (str == endPointer || errno == ERANGE || value > UINT16_MAX) {
        warning("Failed to convert string to u16: \"%s\"", str);
        return 0;
    }

    return (u16) value;
}
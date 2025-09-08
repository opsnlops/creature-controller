
#pragma once

#include "types.h"


/**
 * Convert string to u16
 *
 * @param str to convert
 * @return the u16
 */
u16 stringToU16(const char *str);


/**
 * Helper function to convert a value to a binary string
 *
 * @param value the eight bit value to display
 * @return A string representation of the value in binary
 */
const char* to_binary_string(u8 value);

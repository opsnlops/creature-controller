
#pragma once

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "logging/logging.h"


#include "controller-config.h"


/**
 * Convert string to u16
 *
 * @param str to convert
 * @return the u16
 */
u16 stringToU16(const char* str);

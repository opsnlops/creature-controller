
#pragma once

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdbool>
#include <cerrno>

#include "logging/logging.h"


#include "controller-config.h"


/**
 * Convert string to u16
 *
 * @param str to convert
 * @return the u16
 */
u16 stringToU16(const char* str);

/**
 * @file gpio.h
 * @brief Mock Pico SDK hardware/gpio.h for the host test build.
 *
 * The real header pulls in core SDK types and macros. For unit tests we
 * only need enough of it that controller.h can be parsed; the actual GPIO
 * functions are provided by hardware_mocks.h.
 */

#pragma once

#include "hardware_mocks.h"

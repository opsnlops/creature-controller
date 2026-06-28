/**
 * @file irq.h
 * @brief Mock Pico SDK hardware/irq.h for host tests.
 *
 * Provides just the bits controller.h needs to parse; tests that exercise
 * IRQ behavior should provide their own scaffolding.
 */

#pragma once

#include "hardware_mocks.h"

/* Pico SDK marks ISR functions with __isr; on host builds it is a no-op. */
#ifndef __isr
#define __isr
#endif

/**
 * @file status_lights.h
 * @brief Status light management for system state visualization
 *
 * This module controls WS2812-based RGB LEDs for displaying system state
 * and activity information. The system uses PIO state machines to drive
 * multiple chains of RGB LEDs showing firmware state, I/O activity,
 * and servo position feedback.
 */

#pragma once

#include <FreeRTOS.h>
#include <task.h>

#include "pico/double.h"

#include "controller/config.h"


// This is 0.618033988749895
#define GOLDEN_RATIO_CONJUGATE 0.618033988749895

/**
 * @brief Initialize status lights PIO and state machines
 *
 * Sets up the PIO state machines for driving the WS2812 LED chains on
 * the logic board and servo modules.
 */
void status_lights_init();

/**
 * @brief Start the status lights task
 *
 * Creates and starts the FreeRTOS task that will update the status lights
 * based on system state.
 */
void status_lights_start();

/**
 * @brief Send a pixel color to a specific LED chain
 *
 * Writes a color value to the next LED in the chain controlled by the
 * specified state machine.
 *
 * @param pixel_grb Color value in GRB format (0x00GGRRBB)
 * @param state_machine PIO state machine controlling the target LED chain
 */
void put_pixel(u32 pixel_grb, u8 state_machine);

/**
 * @brief Task function that updates status lights
 *
 * FreeRTOS task that continuously updates the status lights to reflect
 * the current system state.
 *
 * @param pvParameters Task parameters (unused)
 */
portTASK_FUNCTION_PROTO(status_lights_task, pvParameters);
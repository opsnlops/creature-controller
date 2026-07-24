#pragma once

/**
 * @file alive_display.h
 * @brief "Board is alive" LED light show for the Dynamixel Servo Tester
 *
 * The tester doesn't drive real servos, so the servo-module and Dynamixel LED
 * chains normally sit dark. This module animates them with a scrolling rainbow
 * as a FreeRTOS liveness indicator - the modern equivalent of blinking the
 * on-board LED to show the scheduler is running.
 *
 * HW4 only: on older boards these WS2812 chains don't exist, so both functions
 * compile to no-ops and are safe to call unconditionally.
 */

/**
 * @brief Initialize the light-show PIO and LED chains.
 *
 * Claims two state machines on the dedicated PIO, sets up the servo-module and
 * Dynamixel WS2812 chains, and blanks them. Call once before starting the task.
 */
void alive_display_init(void);

/**
 * @brief Start the FreeRTOS task that animates the light show.
 */
void alive_display_start(void);

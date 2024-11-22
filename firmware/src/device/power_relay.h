
#pragma once

#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "controller/config.h"

/**
 * This is a simple abstraction of the power relay on the controller power board!
 *
 * It provides a way to turn the motors on an off on a creature, mostly for safety
 * purposes. (Safety to the creature, not people around it, but I guess it could be
 * used for that, too!)
 */

/**
 * Set up the GPIO pin that the power relay is connected to
 */
void init_power_relay();

/**
 * Power on the relay
 */
void power_relay_on();

/**
 * Power off the relay
 */
void power_relay_off();

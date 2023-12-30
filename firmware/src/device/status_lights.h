
#pragma once

#include "controller-config.h"

#include "pico/double.h"

// This is 0.618033988749895
#define GOLDEN_RATIO_CONJUGATE 0.618033988749895

void status_lights_init();
void status_lights_start();

void put_pixel(u32 pixel_grb, u8 state_machine);

u16 interpolateHue(u16 oldHue, u16 newHue, u8 totalSteps, u8 currentStep);

// Gets the next color in a fairly random order using the golden ratio
double getNextColor(double oldColor);

void seed_random_from_rosc();

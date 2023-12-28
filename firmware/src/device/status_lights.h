
#pragma once

#include "util/fast_hsv2rgb.h"

#include "controller-config.h"

// This is 0.618033988749895
#define GOLDEN_RATIO_CONJUGATE (0.618033988749895 * HSV_HUE_MAX)

void status_lights_init();
void status_lights_start();

void put_pixel(u32 pixel_grb, u8 state_machine);
u32 status_lights_urgb_u32(u8 r, u8 g, u8 b);
u16 interpolateHue(u16 oldHue, u16 newHue, u8 totalSteps, u8 currentStep);
u16 getNextColor(u16 oldColor);

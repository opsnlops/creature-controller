
#pragma once

#include <cstdint>

#if USE_STEPPERS

#include "device/stepper.h"


bool stepper_timer_handler(struct repeating_timer *t);
uint32_t set_ms1_ms2_and_get_steps(StepperState* state);

// Toggle the latch
void inline toggle_latch();


bool home_stepper(uint8_t slot);

#endif
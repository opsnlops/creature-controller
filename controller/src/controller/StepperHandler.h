
#pragma once

#include <cstdint>

#if USE_STEPPERS

#include "controller/Controller.h"
#include "device/Stepper.h"
#include "logging/Logger.h"

class StepperHandler {

  public:
    StepperHandler(std::shared_ptr<creatures::Logger> logger,
                   std::shared_ptr<Controller> controller);

    bool stepper_timer_handler(struct repeating_timer *t);

    uint32_t set_ms1_ms2_and_get_steps(StepperState *state);

    // Toggle the latch
    void inline toggle_latch();

    bool home_stepper(uint8_t slot);

  private:
    std::shared_ptr<creatures::Logger> logger;
    std::shared_ptr<Controller> controller;
};

#endif
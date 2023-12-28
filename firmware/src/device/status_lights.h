
#pragma once


#include "logging/logging.h"
#include "controller/controller.h"
#include "util/fast_hsv2rgb.h"


// This is 0.618033988749895
#define GOLDEN_RATIO_CONJUGATE (0.618033988749895 * HSV_HUE_MAX)

class StatusLights {

public:
    StatusLights(Controller* controller, IOHandler* io);

    void init();
    void start();

    PIO pio;

    uint8_t logic_board_state_machine;
    uint8_t module_a_state_machine;
    uint8_t module_b_state_machine;
    uint8_t module_c_state_machine;

    static inline void put_pixel(uint32_t pixel_grb, uint8_t state_machine);
    static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);

    uint32_t last_input_frame;

    static inline uint16_t interpolateHue(uint16_t oldHue, uint16_t newHue, uint8_t totalSteps, uint8_t currentStep);

    static uint16_t getNextColor(uint16_t oldColor);

    Controller* getController();
    IOHandler* getIOHandler();

private:

    Controller* controller;
    IOHandler* io;

};

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "ws2812.pio.h"

#include "pico/double.h"
#include "pico/stdlib.h"
#include "hardware/regs/rosc.h"

#include "controller/controller.h"
#include "device/colors.h"
#include "device/status_lights.h"
#include "logging/logging.h"
#include "util/ranges.h"



/*
 *
 * Status Light Order:
 *
 * 0 = Firmware State
 * 1 = Running
 * 2 = Servo 0
 * 3..n = Each servo after that
 */


extern volatile u64 position_messages_processed;

u8 logic_board_state_machine;
u8 module_a_state_machine;
u8 module_b_state_machine;
u8 module_c_state_machine;
u32 last_input_frame = 0UL;


// Located in tasks.cpp
extern TaskHandle_t status_lights_task_handle;

extern enum FirmwareState controller_firmware_state;


void put_pixel(uint32_t pixel_grb, uint8_t state_machine) {
    pio_sm_put_blocking(STATUS_LIGHTS_PIO, state_machine, pixel_grb << 8u);
}


void status_lights_init() {

    uint offset = pio_add_program(STATUS_LIGHTS_PIO, &ws2812_program);

    logic_board_state_machine = pio_claim_unused_sm(STATUS_LIGHTS_PIO, true);
    debug("logic board status lights state machine: %u", logic_board_state_machine);
    ws2812_program_init(STATUS_LIGHTS_PIO, logic_board_state_machine, offset, STATUS_LIGHTS_LOGIC_BOARD_PIN, 800000, STATUS_LIGHTS_LOGIC_BOARD_IS_RGBW);

    module_a_state_machine = pio_claim_unused_sm(STATUS_LIGHTS_PIO, true);
    debug("module A state machine: %u", module_a_state_machine);
    ws2812_program_init(STATUS_LIGHTS_PIO, module_a_state_machine, offset, STATUS_LIGHTS_MOD_A_PIN, 800000, STATUS_LIGHTS_MOD_A_IS_RGBW);

    module_b_state_machine = pio_claim_unused_sm(STATUS_LIGHTS_PIO, true);
    debug("module B state machine: %u", module_b_state_machine);
    ws2812_program_init(STATUS_LIGHTS_PIO, module_b_state_machine, offset, STATUS_LIGHTS_MOD_B_PIN, 800000, STATUS_LIGHTS_MOD_B_IS_RGBW);

    module_c_state_machine = pio_claim_unused_sm(STATUS_LIGHTS_PIO, true);
    debug("module C state machine: %u", module_c_state_machine);
    ws2812_program_init(STATUS_LIGHTS_PIO, module_c_state_machine, offset, STATUS_LIGHTS_MOD_C_PIN, 800000, STATUS_LIGHTS_MOD_C_IS_RGBW);

    // Seed the random number generator better so we get better colors on the running light
    seed_random_from_rosc();

}

void status_lights_start() {

    debug("starting the status lights");
    xTaskCreate(status_lights_task,
                "status_lights_task",
                1024,
                NULL,
                1,                      // Low priority
                &status_lights_task_handle);
}



/**
 * @brief Returns the requested step in a fade between two hues
 *
 * This was taken from my old seconds ring clock like what's in the family room :)
 *
 * @param oldColor Starting hue
 * @param newColor Finishing hue
 * @param totalSteps How many steps are we fading
 * @param currentStep Which one to get
 * @return u16 The hue requested
 */
u16 interpolateHue(u16 oldHue, u16 newHue, u8 totalSteps, u8 currentStep) {

    // Convert to floating-point for precise calculation
    float differentialStep = (float)(newHue - oldHue) / totalSteps;
    float stepHue = oldHue + (differentialStep * currentStep);

    // Convert the result back to u16
    return (u16)stepHue;

}


double getNextColor(double oldColor) {

    // https://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically/
    double tempColor = oldColor + GOLDEN_RATIO_CONJUGATE;
    tempColor = fmod(tempColor, 1.0);
    return tempColor;
}

// Read from the queue and print it to the screen for now
portTASK_FUNCTION(status_lights_task, pvParameters) {

    // Define our colors!
    hsv_t idle_color =                  {184.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Like a cyan
    hsv_t configuring_color =           {64.0,  1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Yellowish
    hsv_t running_color =               {127.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Green
    hsv_t running_but_no_data_color =   {241.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Blue
    hsv_t error_color =                 {0.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};       // Red

    TickType_t lastDrawTime;

    // What frame are we on?
    uint64_t frame = 0;

    uint32_t currentIOFrameNumber = 0;
    uint32_t lastIOFrameNumber = 0;
    uint64_t lastIOFrame = 0;

    // Are we currently getting data?
    bool ioActive = false;


    uint32_t statusLightColor = 0;
    uint32_t runningLightColor = 0;

    // Start up with two random colors
    uint16_t currentRunningLightHue = rand() * 360 * 100;
    uint16_t oldRunningLightHue = rand() * 360 * 100;
    uint16_t runningLightFadeStep = 0;
    u16 tempHue = 0;
    hsv_t runningLightHSV;

    uint32_t motorLightColor[MAX_NUMBER_OF_SERVOS + MAX_NUMBER_OF_STEPPERS] = {0};
    uint64_t lastServoFrame[MAX_NUMBER_OF_SERVOS + MAX_NUMBER_OF_STEPPERS] = {0};
    uint16_t currentLightState[MAX_NUMBER_OF_SERVOS + MAX_NUMBER_OF_STEPPERS] = {0};


    // Pre-compute the colors for the system status
    u32 systemStatusIdleColor = hsv_to_urgb(idle_color);
    u32 systemStatusConfiguringColor = hsv_to_urgb(configuring_color);
    u32 systemStatusRunningColor = hsv_to_urgb(running_color);
    u32 systemStatusRunningButNoDataColor = hsv_to_urgb(running_but_no_data_color);
    u32 systemStatusErrorColor = hsv_to_urgb(error_color);


#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        // Make note of now
        lastDrawTime = xTaskGetTickCount();
        frame++;


        /*
         * The first light on the chain is a status light on if we're getting data from the
         * I/O handler
         */

        currentIOFrameNumber = position_messages_processed;

        // Show our state
        if (controller_firmware_state == idle) {
            statusLightColor = systemStatusIdleColor;
        } else if (controller_firmware_state == configuring) {
            statusLightColor = systemStatusConfiguringColor;
        } else if(controller_firmware_state == errored_out) {
            statusLightColor = systemStatusErrorColor;
        } else if(controller_firmware_state == running) {

            /*
             * The running state is a bit more complex. There's two things we could show. Either we're
             * running and receiving data, or we're running but not receiving data. Figure out which
             * color to show here.
             */

            // If we've heard from the IO Handler recently-ish, set the light to running_color (green)
            if (currentIOFrameNumber > lastIOFrameNumber || lastIOFrame + STATUS_LIGHTS_IO_RESPONSIVENESS > frame) {
                // If we entered here because the frame changed, update our info
                if (currentIOFrameNumber > lastIOFrameNumber) {
                    lastIOFrameNumber = currentIOFrameNumber;
                    lastIOFrame = frame;
                }

                statusLightColor = systemStatusRunningColor;

                if (!ioActive) {
                    info("Now receiving data from the IO handler");
                    ioActive = true;
                }
            } else {
                // We haven't heard from the IO handler, so set it running_but_no_data_color (blue)
                statusLightColor = systemStatusRunningButNoDataColor;

                if (ioActive) {
                    warning("Not getting data from the IO handler!");
                    ioActive = false;
                }
            }
        } else {

            // If all else fails, set the light to error_color (red)
            statusLightColor = systemStatusErrorColor;
            warning("Can't set color of status light, unknown state? (%d)", controller_firmware_state);
        }



        /*
         * The second light is an "is running" light, so let's smoothly fade between random colors
         */
        tempHue = interpolateHue(oldRunningLightHue,
                                 currentRunningLightHue,
                                 STATUS_LIGHTS_RUNNING_FRAME_CHANGE,
                                 runningLightFadeStep++);
        runningLightHSV.h = tempHue / 100.0;
        runningLightHSV.s = 1.0;
        runningLightHSV.v = STATUS_LIGHTS_RUNNING_BRIGHTNESS;
        runningLightColor = hsv_to_urgb(runningLightHSV);

        if(runningLightFadeStep > STATUS_LIGHTS_RUNNING_FRAME_CHANGE)
        {
            oldRunningLightHue = currentRunningLightHue;
            currentRunningLightHue = (uint16_t)(getNextColor(currentRunningLightHue / 36000.0) * 36000);
            runningLightFadeStep = 0;
        }



        /*
         * The rest of the lights are the status of the motors
         */
        /*
        for (uint8_t currentServo = 0; currentServo < CONTROLLER_NUM_MODULES * CONTROLLER_MOTORS_PER_MODULE; currentServo++) {

            u16 currentPosition = 512;
            ///uint16_t currentPosition = Controller::getServo(currentServo)->getPosition();

            // Has this one changed?
            if (currentLightState[currentServo] != currentPosition) {
                currentLightState[currentServo] = currentPosition;
                lastServoFrame[currentServo] = frame;
            }

            // If we should be on, what color?
            if (lastServoFrame[currentServo] + STATUS_LIGHTS_MOTOR_OFF_FRAMES > frame) {

                // Convert the position to a hue
                uint16_t hue = convertRange(currentPosition,
                                            MIN_POSITION,
                                            MAX_POSITION,
                                            HSV_HUE_MIN,
                                            HSV_HUE_MAX);

                // Dim slowly until we've hit the limit for when we'd turn off
                uint8_t brightness = convertRange(frame - lastServoFrame[currentServo],
                                                  0,
                                                  STATUS_LIGHTS_MOTOR_OFF_FRAMES,
                                                  0,
                                                  STATUS_LIGHTS_SERVO_BRIGHTNESS);
                brightness = STATUS_LIGHTS_SERVO_BRIGHTNESS - brightness;

                fast_hsv2rgb_32bit(hue, 255, brightness, &r, &g, &b);
                motorLightColor[currentServo] = status_lights_urgb_u32(r, g, b);

            } else {

                // Just turn off
                motorLightColor[currentServo] = 0;
            }
        }
         */

        // Now write out the colors of the lights in one big chunk
        put_pixel(statusLightColor, logic_board_state_machine);
        put_pixel(runningLightColor, logic_board_state_machine);

        /*
        // Dump out the lights to the various modules
        for(uint8_t i = 0; i <= 3; i++) {
            put_pixel(motorLightColor[i], module_a_state_machine);
        }

        for(uint8_t i = 4; i <= 7 != 0; i++) {
            put_pixel(motorLightColor[i], module_b_state_machine);
        }

        for(uint8_t i = 8; i < (CONTROLLER_NUM_MODULES * CONTROLLER_MOTORS_PER_MODULE) && i <= 12; i++) {
            put_pixel(motorLightColor[i], module_c_state_machine);
        }
         */

        // Wait till it's time go again
        vTaskDelayUntil(&lastDrawTime, pdMS_TO_TICKS(STATUS_LIGHTS_TIME_MS));
    }
#pragma clang diagnostic pop
}

/**
 * Seed the random number generator from the ROSC
 *
 * This came from a little gem on the Pi forums:
 *   https://forums.raspberrypi.com/viewtopic.php?t=302960
 */
void seed_random_from_rosc()
{
    u32 random = 0;
    u32 random_bit;
    volatile u32 *rnd_reg = (u32 *)(ROSC_BASE + ROSC_RANDOMBIT_OFFSET);

    for (int k = 0; k < 32; k++) {
        while (1) {
            random_bit = (*rnd_reg) & 1;
            if (random_bit != ((*rnd_reg) & 1)) break;
        }

        random = (random << 1) | random_bit;
    }

    srand(random);
}

#include <limits.h>

#include "ws2812.pio.h"

#include "pico/double.h"
#include "pico/rand.h"

#include "controller/controller.h"
#include "device/colors.h"
#include "device/status_lights.h"
#include "logging/logging.h"
#include "util/ranges.h"

#include "types.h"


/*
 *
 * Status Light Order:
 *  0 = Firmware State
 *  1 = Running
 *  2 = USB
 *  3 = UART
 *
 *
 * Servo Order (per module):
 *  0 = Servo 0
 *  1..n = Each servo after that
 */


volatile TaskHandle_t debug_blinker_task_handle;

volatile u64 position_messages_processed = 0UL;
volatile u64 uart_characters_received = 0UL;
volatile u64 usb_serial_characters_received = 0UL;
volatile u64 uart_messages_sent = 0UL;
volatile u64 uart_messages_received = 0UL;
volatile u64 usb_serial_messages_sent = 0UL;
volatile u64 usb_serial_messages_received = 0UL;
volatile u64 number_of_pwm_wraps = 0UL;


u8 logic_board_state_machine;
u8 module_a_state_machine;
u8 module_b_state_machine;
u8 module_c_state_machine;


TaskHandle_t status_lights_task_handle;
extern enum FirmwareState controller_firmware_state;

// Get access to the motor map from the controller
extern MotorMap motor_map[MOTOR_MAP_SIZE];

void put_pixel(u32 pixel_grb, u8 state_machine) {
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

/**
 * Get a random color based on the golden ratio
 *
 * @param oldColor the old color
 * @return a new random color that's pleasing to the eye
 */
double getNextRandomColor(double oldColor) {

    // https://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically/
    double tempColor = oldColor + GOLDEN_RATIO_CONJUGATE;
    tempColor = fmod(tempColor, 1.0);
    return tempColor;
}

/**
 * Get the next color in the HSV rainbow
 *
 * @param oldColor the old color
 * @return oldColor + IO_LIGHT_COLOR_CYCLE_SPEED or 0.0 if it's past 360
 */
double getNextRainbowColor(double oldColor) {
    double nextColor = oldColor + IO_LIGHT_COLOR_CYCLE_SPEED;
    if(nextColor > 359.9) {
        return 0.0;
    }

   return  (oldColor + IO_LIGHT_COLOR_CYCLE_SPEED);
}

// Read from the queue and print it to the screen for now
portTASK_FUNCTION(status_lights_task, pvParameters) {

    // Define our colors!
    hsv_t idle_color =                  {184.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Like a cyan
    hsv_t configuring_color =           {64.0,  1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Yellowish
    hsv_t running_color =               {127.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Green
    hsv_t running_but_no_data_color =   {241.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};     // Blue
    hsv_t error_color =                 {0.0, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};       // Red
    hsv_t off_color =                   {0.0, 0.0, 0.0};                                                // Off

    TickType_t lastDrawTime;

    // What frame are we on?
    u64 frame = 0;

    u64 currentIOFrameNumber = 0;
    u64 lastIOFrameNumber = 0;
    u64 lastIOFrame = 0;

    u64 lastUARTCharacter = 0;
    u64 lastUSBCharacter = 0;

    // Are we currently getting data?
    bool ioActive = false;


    u32 statusLightColor;
    u32 runningLightColor;
    u32 uartLightColor;
    u32 usbLightColor;

    // Start up with two random colors
    u16 currentRunningLightHue = convertRange(get_rand_32(), 0, UINT32_MAX, 1000, 360 * 100);
    u16 oldRunningLightHue = convertRange(get_rand_32(), 0, UINT32_MAX, 1000, 360 * 100);
    u16 runningLightFadeStep = 0;
    u16 tempHue = 0;
    hsv_t runningLightHSV;

    u32 motorLightColor[MOTOR_MAP_SIZE] = {0};
    u64 lastServoFrame[MOTOR_MAP_SIZE] = {0};
    u16 currentLightState[MOTOR_MAP_SIZE] = {0};


    // Pre-compute the colors for the system status
    u32 systemStatusIdleColor = hsv_to_urgb(idle_color);
    u32 systemStatusConfiguringColor = hsv_to_urgb(configuring_color);
    u32 systemStatusRunningColor = hsv_to_urgb(running_color);
    u32 systemStatusRunningButNoDataColor = hsv_to_urgb(running_but_no_data_color);
    u32 systemStatusErrorColor = hsv_to_urgb(error_color);
    u32 offColor = hsv_to_urgb(off_color);

    double uartLightHue = 0.0;
    double usbLightHue = 0.0;

    lastUARTCharacter = uart_characters_received;
    lastUSBCharacter = usb_serial_characters_received;

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
            currentRunningLightHue = (u16)(getNextRandomColor(currentRunningLightHue / 36000.0) * 36000);
            runningLightFadeStep = 0;
        }


        /*
         * The third light is a "have I seen stuff on the USB lately" light
         */
        if((usb_serial_characters_received > lastUSBCharacter))
        {
            usbLightHue = getNextRainbowColor(usbLightHue);
            hsv_t tempHsv = {usbLightHue, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};
            usbLightColor = hsv_to_urgb(tempHsv);
        } else {
            usbLightColor = offColor;
        }
        lastUSBCharacter = usb_serial_characters_received;


        /*
         * The fourth light is a "have I seen stuff on the UART lately" light
         */   // 60                             50
        if((uart_characters_received > lastUARTCharacter))
        {
            uartLightHue = getNextRainbowColor(uartLightHue);
            hsv_t tempHsv = {uartLightHue, 1.0, STATUS_LIGHTS_SYSTEM_STATE_STATUS_BRIGHTNESS};
            uartLightColor = hsv_to_urgb(tempHsv);
        } else {
            uartLightColor = offColor;
        }
        lastUARTCharacter = uart_characters_received;




        /*
         * The rest of the lights are the status of the motors
         */

        for (u8 currentServo = 0; currentServo < MOTOR_MAP_SIZE; currentServo++) {

            // Get a copy of the current state in case it mutates while we're processing things
            u16 currentPosition = motor_map[currentServo].current_microseconds;
            u16 minPosition = motor_map[currentServo].min_microseconds;
            u16 maxPosition = motor_map[currentServo].max_microseconds;

            // Has this one changed?
            if (currentLightState[currentServo] != currentPosition) {
                currentLightState[currentServo] = currentPosition;
                lastServoFrame[currentServo] = frame;
            }

            // If we should be on, what color?
            if (lastServoFrame[currentServo] + STATUS_LIGHTS_MOTOR_OFF_FRAMES > frame) {

                // Convert the position to a hue
                u16 hue = convertRange(currentPosition,
                                       minPosition,
                                       maxPosition,
                                       0,              // 0 is red
                                       23300);         // 233 * 100 (233 is blue)

                // Dim slowly until we've hit the limit for when we'd turn off
                u8 brightness = convertRange(frame - lastServoFrame[currentServo],
                                                  0,
                                                  STATUS_LIGHTS_MOTOR_OFF_FRAMES,
                                                  0,
                                                  STATUS_LIGHTS_SERVO_BRIGHTNESS);
                brightness = STATUS_LIGHTS_SERVO_BRIGHTNESS - brightness;

                // Make this into a color we can show
                hsv_t hsv;
                hsv.h = (double)((double)hue / 100.0);
                hsv.s = 1.0;
                hsv.v = (double)((double)brightness / UINT8_MAX);
                motorLightColor[currentServo] = hsv_to_urgb(hsv);

            } else {

                // Just turn off
                motorLightColor[currentServo] = 0;
            }
        }

        // Now write out the colors of the lights in one big chunk
        put_pixel(statusLightColor, logic_board_state_machine);
        put_pixel(runningLightColor, logic_board_state_machine);
        put_pixel(usbLightColor, logic_board_state_machine);
        put_pixel(uartLightColor, logic_board_state_machine);

        // Dump out the lights to the various modules
        for(u8 i = 0; i < MOTOR_MAP_SIZE; i++) {

            if(i < CONTROLLER_MOTORS_PER_MODULE) {
                put_pixel(motorLightColor[i], module_a_state_machine);
            } else if(i < CONTROLLER_MOTORS_PER_MODULE * 2) {
                put_pixel(motorLightColor[i], module_b_state_machine);
            } else {
                put_pixel(motorLightColor[i], module_c_state_machine);
            }
        }

        // Wait till it's time go again
        vTaskDelayUntil(&lastDrawTime, pdMS_TO_TICKS(STATUS_LIGHTS_TIME_MS));
    }
#pragma clang diagnostic pop
}

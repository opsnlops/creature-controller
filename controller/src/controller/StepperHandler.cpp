
#include  <atomic>

#include "controller-config.h"

#include "controller/Controller.h"

#if USE_STEPPERS

#include "controller/StepperHandler.h"

//
// START OF STEPPER TIMER STUFFS
//

std::atomic<u64> stepper_frame_count = 0L;
std::atomic<u64> time_spent_in_stepper_handler = 0L;

/**
 * Simple array for setting the address lines of the stepper latches
 */
static bool stepperAddressMapping[MAX_NUMBER_OF_STEPPERS][STEPPER_MUX_BITS] = {

        {false,     false,      false},     // 0
        {false,     false,      true},      // 1
        {false,     true,       false},     // 2
        {false,     true,       true},      // 3
        {true,      false,      false},     // 4
        {true,      false,      true},      // 5
        {true,      true,       false},     // 6
        {true,      true,       true}       // 7
};

//
// END OF STEPPER TIMER STUFFS
//


StepperHandler::StepperHandler(std::shared_ptr<creatures::Logger> logger, std::shared_ptr<Controller> controller) :
    logger(logger), controller(controller) {
    logger->debug("new StepperHandler made");
}

void inline StepperHandler::toggle_latch() {
    // Enable the latch
    // TODO: Switch these gpios
    //gpio_put(STEPPER_LATCH_PIN, false);     // It's active low

    // Stall long enough to let the latch go! This about 380ns. The datasheet says it
    // needs 220ns to latch at 2v. (We run at 3.3v) The uint32_t executes faster than an
    // uint8_t! It surprised me to figure this out. :)
    u32 j;
    for(j = 0; j < 3; j += 1) {}

    // Now that we've toggled everything, turn the latch back off
    //gpio_put(STEPPER_LATCH_PIN, true);     // It's active low
}


/*
 * Truth Table for the A3967 Stepper (this is the EasyDriver one!)
 *
 *     +------------------------------+
 *     |  MS1  |  MS2  |  Resolution  |
 *     |-------|-------|--------------|
 *     |   L   |   L   | Full step    |
 *     |   H   |   L   | Half step    |
 *     |   L   |   H   | Quarter step |
 *     |   H   |   H   | Eighth step  |
 *     +------------------------------+
 *
 */



/**
 *
 * Callback for the stepper timer
 *
 * REMEMBER THAT THIS RUNS EVERY FEW MICROSECONDS! :)
 *
 * @param t the repeating timer
 * @return true
 */
bool StepperHandler::stepper_timer_handler(struct repeating_timer *t) {

    // Let's keep some metrics of how long this takes
    // TODO: Chrono action time
    uint64_t start_time = 0;
    //uint64_t start_time = time_us_64();

    // Keep track of which frame we're in
    stepper_frame_count += 1;


    // Look at each stepper we have and adjust if needed
    for(int i = 0; i < controller->getNumberOfSteppersInUse(); i++) {

        Stepper *s = controller->getStepper(i);
        uint8_t slot = s->getSlot();


        /*
         * Load this stepper's state
         */
        StepperState* state = s->state;

        uint32_t microSteps;

        // TODO: This isn't a great way to handle this
        if(state->lowEndstop) {
            logger->error("Low endstop hit on stepper {}", slot);
            state->isAwake = false;
            state->startedSleepingAt = stepper_frame_count;
            goto transmit;
        }

        if(state->highEndstop) {
            logger->error("High endstop hit on stepper {}", slot);
            state->isAwake = false;
            state->startedSleepingAt = stepper_frame_count;
            goto transmit;
        }

        // If this stepper is high, there's nothing else to do. Set it to low.
        if(state->isHigh) {
            state->isHigh = false;
            goto transmit;
        }

        // If we're waking up, but we haven't had enough frames yet to wake up, keep on waiting
        if(state->awakeAt > stepper_frame_count) {
            goto end;
        }

        // Should we go to sleep?
        if(state->isAwake && state->updatedFrame + state->sleepAfterIdleFrames < stepper_frame_count) {
            state->isAwake = false;
            state->startedSleepingAt = stepper_frame_count;
            logger->debug("sleeping stepper {} at frame {}", slot, stepper_frame_count);
            goto transmit;

        }

        // If we're at the position where we need to be, stop
        if(state->currentMicrostep == state->desiredMicrostep && !state->moveRequested) {
            goto end;
        }

        // If we're asleep, but we should wake up, now's the time. We need to move.
        if(!state->isAwake) {
            state->isAwake = true;
            state->awakeAt = stepper_frame_count + state->framesRequiredToWakeUp;
            logger->debug("waking up stepper {} at frame {}", slot, stepper_frame_count);
            goto transmit;
        }

        /*
         * If we are on a whole step boundary, update the requested microsteps!
         *
         * This is only done on whole step boundaries to avoid drift. If we don't, the
         * stepper will drift (pretty badly) over time.
         */
        if(state->currentMicrostep % STEPPER_MICROSTEP_MAX == 0)
            state->desiredMicrostep = state->requestedSteps * STEPPER_MICROSTEP_MAX;


        /*
         * So now we know we need to move. Let's figure out which direction.
         */

        if( state->currentMicrostep < state->desiredMicrostep ) {

            microSteps = set_ms1_ms2_and_get_steps(state);

            state->currentDirection = false;
            state->currentMicrostep = state->currentMicrostep + microSteps;
            state->isHigh = true;
            state->actualSteps += microSteps;

            goto transmit;

        }

        // The only thing left is to move the other way

        microSteps = set_ms1_ms2_and_get_steps(state);

        state->currentDirection = true;
        state->currentMicrostep = state->currentMicrostep - microSteps;
        state->isHigh = true;
        state->actualSteps += microSteps;

        goto transmit;




        /*
         * Get the state of the latch for this stepper to match what we think it is
         */


        transmit:

        // Configure the address lines
        // TODO: Pi GPIOs, please
        //gpio_put(STEPPER_A0_PIN, stepperAddressMapping[slot][2]);
        //gpio_put(STEPPER_A1_PIN, stepperAddressMapping[slot][1]);
        //gpio_put(STEPPER_A2_PIN, stepperAddressMapping[slot][0]);

        //gpio_put(STEPPER_DIR_PIN, state->currentDirection);
        //gpio_put(STEPPER_STEP_PIN, state->isHigh);
        //gpio_put(STEPPER_MS1_PIN, state->ms1State);
        //gpio_put(STEPPER_MS2_PIN, state->ms2State);
        //gpio_put(STEPPER_SLEEP_PIN, state->isAwake);        // Sleep is active low

        // Toggle the latch so we make this go
        toggle_latch();

        state->moveRequested = false;
        state->updatedFrame = stepper_frame_count;

        // Check the endstops
        //state->lowEndstop = gpio_get(STEPPER_END_S_LOW_PIN);
        //state->highEndstop = gpio_get(STEPPER_END_S_HIGH_PIN);

        end:
        (void)nullptr;

    }

    // Account for the time spent in here
    // TODO: Chorns time
    //time_spent_in_stepper_handler += time_us_64() - start_time;

    return true;
}


uint32_t StepperHandler::set_ms1_ms2_and_get_steps(StepperState* state) {

    uint32_t stepsToGo = (state->currentMicrostep > state->desiredMicrostep) ?
                         (state->currentMicrostep - state->desiredMicrostep) :
                         (state->desiredMicrostep - state->currentMicrostep);

    uint32_t microSteps = STEPPER_SPEED_0_MICROSTEPS;

    // A setting of "0" means no deceleration, so set full steps
    if (state->decelerationAggressiveness == 0) {
        state->ms1State = false;
        state->ms2State = false;
        goto end;
    }

    // Do full steps
    if(stepsToGo > (STEPPER_MICROSTEP_MAX * state->decelerationAggressiveness * 8)) {
        state->ms1State = false;
        state->ms2State = false;
        microSteps = STEPPER_SPEED_0_MICROSTEPS;
        goto end;
    }

    if(stepsToGo > (STEPPER_MICROSTEP_MAX * state->decelerationAggressiveness * 4)) {
        state->ms1State = true;
        state->ms2State = false;

        microSteps = STEPPER_SPEED_1_MICROSTEPS;
        goto end;
    }

    if(stepsToGo > (STEPPER_MICROSTEP_MAX * state->decelerationAggressiveness * 2)) {
        state->ms1State = false;
        state->ms2State = true;

        microSteps = STEPPER_SPEED_2_MICROSTEPS;
        goto end;
    }

    state->ms1State = true;
    state->ms2State = true;
    microSteps = STEPPER_SPEED_3_MICROSTEPS;
    goto end;



    end:
    return microSteps;

}

/**
* Moves the stepper to the low end-stop safely
 *
 * This uses the main CPU to do all of the timing, since we need to move and check
 * the endstops very exactly. Once this is done the controller will hand over
 * control of things to the normal handler, but to get into a known state we need
 * to do it nice and slow.
*
* @param slot the stepper to home
*/
bool StepperHandler::home_stepper(uint8_t slot) {

    logger->info("attempting to home stepper {}", slot);

    // Set up the address lines for the stepper we're looking at
    // TODO: GPIO
    //gpio_put(STEPPER_A0_PIN, stepperAddressMapping[slot][2]);
    //gpio_put(STEPPER_A1_PIN, stepperAddressMapping[slot][1]);
    //gpio_put(STEPPER_A2_PIN, stepperAddressMapping[slot][0]);


    //gpio_put(STEPPER_STEP_PIN, false);
    //gpio_put(STEPPER_DIR_PIN, false);
    //gpio_put(STEPPER_MS1_PIN, true);
    //gpio_put(STEPPER_MS2_PIN, true);
    //gpio_put(STEPPER_SLEEP_PIN, true);


    logger->debug("waking up stepper {}", slot);

    // Set this on the latches
    toggle_latch();

    // This is way longer than we actually need, but let's be safe!
    // TODO: Thread.sleep
    //vTaskDelay(pdMS_TO_TICKS(500));


    uint32_t steps_moved = 0;

    /*
    bool high = false;
    bool home_reached = gpio_get(STEPPER_END_S_LOW_PIN);
    while(!home_reached) {

        if(++steps_moved % 100 == 0) {
            debug("moved stepper {} {} microsteps", slot, steps_moved);
        }

        // Let's slowly walk backwards
        gpio_put(STEPPER_STEP_PIN, high);

        toggle_latch();
        vTaskDelay(pdMS_TO_TICKS(4));

        high = !high;

        home_reached = gpio_get(STEPPER_END_S_LOW_PIN);

        if(home_reached)
            info("stepper {} home reached", slot);
    }

    // Pause to set all movement settle
    debug("pausing...");
    vTaskDelay(pdMS_TO_TICKS(2000));


    // Now let's walk the stepper forward a little bit
    gpio_put(STEPPER_DIR_PIN, true);
    gpio_put(STEPPER_MS1_PIN, true);
    gpio_put(STEPPER_MS2_PIN, true);

    high = true;
    for(int i = 0; i < 3 * STEPPER_MICROSTEP_MAX; i++) {

        vTaskDelay(pdMS_TO_TICKS(6));

        gpio_put(STEPPER_STEP_PIN, high);
        toggle_latch();
        high = !high;
    }

    // We're done, pause for a moment before going on
    vTaskDelay(pdMS_TO_TICKS(2000));
    debug("done with stepper {}", slot);

    return home_reached;
     */
    return true;

}

#endif
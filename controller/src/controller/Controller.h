
#pragma once

#include <memory>

#include "controller-config.h"

#include "device/Servo.h"
#include "logging/Logger.h"

#if USE_STEPPERS
#include "device/Stepper.h"
#endif

// I've got a dependency loop. Let's use forward declaration to
// break it.
class Creature; // Forward declaration

class Controller {

public:
    Controller(std::shared_ptr<creatures::Logger> logger);

    u32 getNumberOfPWMWraps();
    u16 getServoPosition(u8 outputPin);

    void requestServoPosition(u8 outputPin, u16 requestedPosition);

    void init(std::shared_ptr<Creature> creature);
    void start();

    //void setCreatureWorkerTaskHandle(TaskHandle_t creatureWorkerTaskHandle);

    void powerOn();
    void powerOff();
    void powerToggle();
    [[nodiscard]] bool isPoweredOn();

    [[nodiscard]] bool hasReceivedFirstFrame();
    void confirmFirstFrameReceived();

    u8* getCurrentFrame();

    u8 getPinMapping(u8 servoNumber);

    bool acceptInput(u8* input);

    u8 getNumberOfServosInUse();

    [[nodiscard]] u16 getNumberOfDMXChannels();

    [[nodiscard]] bool isOnline();
    void setOnline(bool onlineValue);

    // Get the servo, used for debugging
    Servo* getServo(u8 outputPin);

#if USE_STEPPERS
    Stepper* getStepper(u8 index);
    u8 getNumberOfSteppersInUse();
    u32 getStepperPosition(u8 indexNumber);
    void requestStepperPosition(u8 stepperIndexNumber, u32 requestedPosition);
#endif

    // ISR, called when the PWM wraps
    //static void __isr on_pwm_wrap_handler();

private:

    std::shared_ptr<Creature> creature;
    std::shared_ptr<creatures::Logger> logger;


    /**
     * An array of all of the servos we have. Set to the max number possible,
     * and then we wait for whatever creature is this to init the ones it
     * intends to use.
     */
    static Servo* servos[MAX_NUMBER_OF_SERVOS];

    /**
     * Keeps track of if we are considered "online."
     *
     * When the controller is online, it will process input from the I/O handler.
     * If the controller is offline, it throws away the input from the handler, which
     * also makes it not call the housekeeping task.
     *
     * This is used for debugging mostly. It allows the debug shell to set a direct a
     * value of ticks to the servo directly. (Which is good for determining limits.)
     */
    bool online = true;

    /**
     * Have we received a frame off the wire?
     *
     * This is used to if it's safe to turn on the motors. To avoid having the servos jump
     * to what might be an invalid position, and risking damaging things, don't turn on
     * the power until we've gotten good data from the wire.
     */
    bool receivedFirstFrame = false;

    // The current state of the input from the controller
    u8* currentFrame{};

    // How many channels we're expecting from the I/O handler
    u16 numberOfChannels;

    /**
     * A handle to our creature's working task. Used to signal that a new
     * frame has been received off the wire.
     *
     * TODO: Thread this
     */
    //TaskHandle_t creatureWorkerTaskHandle;

    // The ISR needs access to these values
    static u8 numberOfServosInUse;
    static u32 numberOfPWMWraps;

    void configureGPIO(u8 pin, bool out, bool initialValue);

    void initServo(u8 indexNumber, const char* name, u16 minPulseUs,
                   u16 maxPulseUs, float smoothingValue, u16 defaultPosition, bool inverted);

#if USE_STEPPERS
    void initStepper(u8 indexNumber, const char* name, u32 maxSteps, u16 decelerationAggressiveness,
                     u32 sleepWakeupPauseTimeUs, u32 sleepAfterUs, bool inverted);

    static Stepper* steppers[MAX_NUMBER_OF_STEPPERS];
    static u8 numberOfSteppersInUse;
#endif

    /**
     * Map the servo index to the GPO pin to use
     */
    u8 pinMappings[MAX_NUMBER_OF_SERVOS] = {
            SERVO_0_GPIO_PIN,
            SERVO_1_GPIO_PIN,
            SERVO_2_GPIO_PIN,
            SERVO_3_GPIO_PIN,
            SERVO_4_GPIO_PIN,
            SERVO_5_GPIO_PIN,
            SERVO_6_GPIO_PIN,
            SERVO_7_GPIO_PIN};

};
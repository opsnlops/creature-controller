
#pragma once

#include <memory>

#include "namespace-stuffs.h"
#include "controller-config.h"

#include "creature/config.h"
#include "device/relay.h"
#include "device/servo.h"

#if USE_STEPPERS
#include "device/stepper.h"
#endif

class Controller {

public:
    Controller();

    static uint32_t getNumberOfPWMWraps();
    uint16_t getServoPosition(u8 outputPin);

    void requestServoPosition(u8 outputPin, u16 requestedPosition);

    void init(CreatureConfig* incomingConfig);
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

    static u8 getNumberOfServosInUse();

    [[nodiscard]] u16 getNumberOfDMXChannels();

    [[nodiscard]] bool isOnline();
    void setOnline(bool onlineValue);

    // Get the servo, used for debugging
    static Servo* getServo(u8 outputPin);

#if USE_STEPPERS
    static Stepper* getStepper(u8 index);
    static uint8_t getNumberOfSteppersInUse();
    uint32_t getStepperPosition(u8 indexNumber);
    static void requestStepperPosition(u8 stepperIndexNumber, u32 requestedPosition);
#endif

    // ISR, called when the PWM wraps
    //static void __isr on_pwm_wrap_handler();

private:
    bool poweredOn = false;
    Relay* powerRelay;

    // The configuration to use
    CreatureConfig* config;

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
    uint8_t* currentFrame{};

    // How many channels we're expecting from the I/O handler
    uint16_t numberOfChannels;

    /**
     * A handle to our creature's working task. Used to signal that a new
     * frame has been received off the wire.
     *
     * TODO: Thread this
     */
    //TaskHandle_t creatureWorkerTaskHandle;

    // The ISR needs access to these values
    static uint8_t numberOfServosInUse;
    static uint32_t numberOfPWMWraps;

    static void configureGPIO(uint8_t pin, bool out, bool initialValue);

    void initServo(uint8_t indexNumber, const char* name, uint16_t minPulseUs,
                   uint16_t maxPulseUs, float smoothingValue, uint16_t defaultPosition, bool inverted);

#if USE_STEPPERS
    void initStepper(uint8_t indexNumber, const char* name, uint32_t maxSteps, uint16_t decelerationAggressiveness,
                     uint32_t sleepWakeupPauseTimeUs, uint32_t sleepAfterUs, bool inverted);

    static Stepper* steppers[MAX_NUMBER_OF_STEPPERS];
    static uint8_t numberOfSteppersInUse;
#endif

    /**
     * Map the servo index to the GPO pin to use
     */
    uint8_t pinMappings[MAX_NUMBER_OF_SERVOS] = {
            SERVO_0_GPIO_PIN,
            SERVO_1_GPIO_PIN,
            SERVO_2_GPIO_PIN,
            SERVO_3_GPIO_PIN,
            SERVO_4_GPIO_PIN,
            SERVO_5_GPIO_PIN,
            SERVO_6_GPIO_PIN,
            SERVO_7_GPIO_PIN};

};
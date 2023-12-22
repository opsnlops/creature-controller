
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "controller-config.h"

#include "controller/commands/ICommand.h"

#include "device/Servo.h"
#include "logging/Logger.h"
#include "io/SerialHandler.h"

#if USE_STEPPERS
#include "device/Stepper.h"
#endif

// I've got a dependency loop. Let's use forward declaration to
// break it.
class Creature; // Forward declaration

class Controller {

public:
    Controller(std::shared_ptr<creatures::Logger> logger);

    u16 getServoPosition(u8 outputPin);

    void init(std::shared_ptr<Creature> creature, std::shared_ptr<creatures::SerialHandler> serialHandler);
    void start();

    //void setCreatureWorkerTaskHandle(TaskHandle_t creatureWorkerTaskHandle);

    /**
     * Enqueue a command to send to the creature
     *
     * @param command an instance of `ICommand` to send
     */
    void sendCommand(const std::shared_ptr<creatures::ICommand>& command);

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

private:

    std::shared_ptr<Creature> creature;
    std::shared_ptr<creatures::Logger> logger;
    std::shared_ptr<creatures::SerialHandler> serialHandler;


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

};

#pragma once

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "controller-config.h"

#include "controller/Input.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/SetServoPositions.h"
#include "creature/Creature.h"

#include "device/Servo.h"
#include "logging/Logger.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

#if USE_STEPPERS
#include "device/Stepper.h"
#endif

// I've got a dependency loop. Let's use forward declaration to
// break it.
namespace creatures::creature {
    class Creature; // Forward declaration
}

namespace creatures::io {
    class MessageRouter; // Forward declaration
}


class Controller : public creatures::StoppableThread {

public:
    explicit Controller(const std::shared_ptr<creatures::Logger>& logger,
                        const std::shared_ptr<creatures::creature::Creature>& creature,
                        std::shared_ptr<creatures::io::MessageRouter> messageRouter);

    void start() override;


    /**
     * Enqueue a command to send to the creature
     *
     * @param command an instance of `ICommand` to send
     * @param destModule the module to send the command to
     */
    void sendCommand(const std::shared_ptr<creatures::ICommand>& command,
                     creatures::config::UARTDevice::module_name destModule);


    /**
     * Send a special request to the firmware to flush its buffer
     */
    void sendFlushBufferRequest();

    void powerOn();
    void powerOff();
    void powerToggle();
    [[nodiscard]] bool isPoweredOn();

    [[nodiscard]] bool hasReceivedFirstFrame();
    void confirmFirstFrameReceived();



    /**
     * Accept input from an input handler
     *
     * @param inputs a `std::vector<creatures::Input>` of input objects
     * @return true if it worked
     */
    bool acceptInput(const std::vector<creatures::Input>& inputs);

    /**
     * @brief Get a reference to the input queue
     *
     * @return a `std::shared_ptr<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>>` 😅
     */
    std::shared_ptr<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>> getInputQueue();

    u8 getNumberOfServosInUse();

    [[nodiscard]] u16 getNumberOfDMXChannels();

    [[nodiscard]] bool isOnline();
    void setOnline(bool onlineValue);


    /**
     * @brief Gets a shared pointer to our creature
     *
     * @return std::shared_ptr<creatures::creature::Creature>
     */
    std::shared_ptr<creatures::creature::Creature> getCreature();


    /**
     * @brief Informs the controller that the firmware is ready for initialization
     *
     * This is called by the InitHandler when we get a message from the firmware that it's
     * showtime!
     */
    void firmwareReadyForInitialization(u32 firmwareVersion);

    /**
     * @brief Tells the controller that the firmware is ready to operate
     *
     * This is set by ReadyHandler.
     */
    void firmwareReadyToOperate();

#if USE_STEPPERS
    Stepper* getStepper(u8 index);
    u8 getNumberOfSteppersInUse();
    u32 getStepperPosition(u8 indexNumber);
    void requestStepperPosition(u8 stepperIndexNumber, u32 requestedPosition);
#endif

protected:
    void run() override;

private:

    std::shared_ptr<creatures::creature::Creature> creature;
    std::shared_ptr<creatures::Logger> logger;
    std::shared_ptr<creatures::io::MessageRouter> messageRouter;

    std::atomic<u64> number_of_frames = 0UL;

    /**
     * Our queue of inputs from the I/O handlers. A reference to this
     * queue is made available to the creature.
     */
    std::shared_ptr<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>> inputQueue;


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
     * Don't transmit anything to the firmware if we haven't gotten a signal from the
     * I/O handler. It's not good to send junk frames to the firmware. It _should_ discard
     * anything that doesn't make sense, but do we really wanna trust it?
     */
    bool receivedFirstFrame = false;

    /**
     * Has the firmware told us that it's ready?
     *
     * This is set via a READY message from the firmware.
     */
    bool firmwareReady = false;

    // How many channels we're expecting from the I/O handler
    u16 numberOfChannels;

};
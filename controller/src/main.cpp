
#include <cstdlib>
#include <thread>

#include "controller-config.h"

#include "config/CommandLine.h"
#include "config/Configuration.h"
#include "config/CreatureBuilder.h"
#include "controller/Controller.h"
#include "controller/tasks/PingTask.h"
#include "creature/Creature.h"
#include "device/Servo.h"
#include "dmx/E131Server.h"
#include "io/SerialHandler.h"
#include "io/MessageProcessor.h"
#include "logging/Logger.h"
#include "logging/SpdlogLogger.h"
#include "util/thread_name.h"

#include "controller/commands/SetServoPositions.h"
#include "controller/commands/tokens/ServoPosition.h"

int main(int argc, char **argv) {

    // Name the thread so it shows up right!
    setThreadName("main");

    // Get the logger up and running ASAP
    std::shared_ptr<creatures::Logger> logger = std::make_shared<creatures::SpdlogLogger>();
    logger->init("controller");

    logger->info("Welcome to the Creature Controller! 🦜");

    // Leave some version info to be found
    logger->debug("spdlog version {}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    logger->debug("fmt version {}", FMT_VERSION);


    // Parse out the command line options
    auto commandLine = std::make_unique<creatures::CommandLine>(logger);
    auto config = commandLine->parseCommandLine(argc, argv);

    auto builder = creatures::CreatureBuilder(logger, creatures::CreatureBuilder::fileToStream(logger, config->getConfigFileName()));
    auto creature = builder.build();


    // Hooray, we did it!
    logger->info("working with {}! ({})", creature->getName(), creature->getDescription());
    logger->debug("{} has {} servos and {} steppers", creature->getName(),
          creature->getNumberOfServos(), creature->getNumberOfSteppers());

    // Start up the SerialHandler
    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>(logger, config->getUsbDevice(), outgoingQueue, incomingQueue);
    serialHandler->start();

    // Fire up the MessageProcessor
    auto messageProcessor = std::make_shared<creatures::MessageProcessor>(logger, serialHandler);
    messageProcessor->start();

    // Fire up the controller
    auto controller = std::make_shared<Controller>(logger);
    controller->init(creature, serialHandler);
    controller->start();

    // Now that the controller is running, we can start the creature
    creature->init(controller);
    creature->start();

    // Create and start the servo controller
    // Create and start the e1.13 server
    logger->debug("starting the e1.13 server");
    auto e131Server = std::make_shared<creatures::dmx::E131Server>(logger);
    e131Server->init(creature, controller);
    e131Server->start();

    // Fire up the ping task
    auto pingTask = std::make_shared<creatures::tasks::PingTask>(logger);
    pingTask->init(serialHandler);
    pingTask->start();


#if 0

    // Send 1000 messages for testing at our normal pacing of 20ms per
    logger->info("starting the servo test");
    for(u16 i = 1000; i < 8000; i = i + 50) {

        auto command = std::make_shared<creatures::commands::SetServoPositions>(logger);
        command->addServoPosition(creatures::ServoPosition("A0", i));
        command->addServoPosition(creatures::ServoPosition("A1", i+10));
        command->addServoPosition(creatures::ServoPosition("A2", i+20));
        command->addServoPosition(creatures::ServoPosition("A3", i+30));
        command->addServoPosition(creatures::ServoPosition("B0", i+10));
        command->addServoPosition(creatures::ServoPosition("B1", i+20));
        command->addServoPosition(creatures::ServoPosition("B2", i+30));
        command->addServoPosition(creatures::ServoPosition("B3", i+40));

        controller->sendCommand(command);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    logger->info("done with the servo test");
#endif
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for(EVER){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#pragma clang diagnostic pop

    logger->info("the main thread says bye! good luck little threads! 👋🏻");
    return EXIT_SUCCESS;
}

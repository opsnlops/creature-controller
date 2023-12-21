
#include <cstdlib>
#include <thread>

#include "controller-config.h"

#include "config/CommandLine.h"
#include "config/Configuration.h"
#include "config/CreatureBuilder.h"
#include "creature/Creature.h"
#include "device/Servo.h"
#include "dmx/e131_server.h"
#include "io/SerialHandler.h"
#include "io/MessageProcessor.h"
#include "logging/Logger.h"
#include "logging/SpdlogLogger.h"
#include "util/thread_name.h"



// Declare the configuration globally
std::shared_ptr<Creature> creature;
std::shared_ptr<creatures::Configuration> config;
std::shared_ptr<creatures::E131Server> e131Server;
std::mutex servoUpdateMutex;

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
    config = commandLine->parseCommandLine(argc, argv);

    auto builder = creatures::CreatureBuilder(logger, creatures::CreatureBuilder::fileToStream(logger, config->getConfigFileName()));
    creature = builder.build();

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


    // Create and start the e1.13 server
    logger->debug("starting the e1.13 server");
    e131Server = std::make_shared<creatures::E131Server>(logger);
    e131Server->start();



    // Set a signal on pin 0 for testing
    /*
    for(u16 i = 4000; i > 200; i = i - 20) {
        info("setting pin 0 to start: 0, stop: {}", i);
        servoController->setPin(0, i, false);
        servoController->setPin(15, i, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    servoController->setPWM(0, 0, 1000);
    servoController->setPWM(15, 0, 2000);

    info("pin 15 PWM is on: {}, off: {}", servoController->getPWM(15, false), servoController->getPWM(15, true));

    trace("sleeping for 2 seconds");
    std::this_thread::sleep_for(std::chrono::seconds(2));
     */



    //i2cBus->close();

    while(true){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    logger->info("the main thread says bye! good luck little threads! 👋🏻");
    return EXIT_SUCCESS;
}

#include <chrono>
#include <cstdlib>
#include <locale>
#include <thread>


#include "controller-config.h"
#include "namespace-stuffs.h"
#include "pca9685/pca9685.h"

#include "config/command-line.h"
#include "config/config.h"
#include "config/creature_builder.h"
#include "creature/creature.h"
#include "device/i2c_mock.h"
#include "device/servo.h"
#include "dmx/e131_server.h"
#include "io/SerialHandler.h"
#include "io/MessageProcessor.h"


// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"


#include "device/i2c_servo/i2c_servo.h"
#include "device/i2c_smbus.h"

// Declare the configuration globally
std::shared_ptr<Creature> creature;
std::shared_ptr<creatures::Configuration> config;
std::shared_ptr<creatures::E131Server> e131Server;
std::mutex servoUpdateMutex;

int main(int argc, char **argv) {

    try {
        // Set up our locale. If this vomits, install `locales-all`
        std::locale::global(std::locale("en_US.UTF-8"));
    }
    catch (const std::runtime_error &e) {
        critical("Unable to set the locale: '{}' (Hint: Make sure package locales-all is installed!)", e.what());
        return EXIT_FAILURE;
    }

    // Console logger
    spdlog::set_level(spdlog::level::trace);

    info("Welcome to the Creature Controller! 🦜");

    // Leave some version info to be found
    debug("spdlog version {}.{}.{}", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
    debug("fmt version {}", FMT_VERSION);


    // Parse out the command line options
    config = creatures::CommandLine::parseCommandLine(argc, argv);
    auto builder = creatures::CreatureBuilder(creatures::CreatureBuilder::fileToStream(config->getConfigFileName()));
    creature = builder.build();

    // Hooray, we did it!
    info("working with {}! ({})", creature->getName(), creature->getDescription());
    debug("{} has {} servos and {} steppers", creature->getName(),
          creature->getNumberOfServos(), creature->getNumberOfSteppers());

    // Start up the SerialHandler
    auto outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto serialHandler = std::make_shared<creatures::SerialHandler>(config->getUsbDevice(), outgoingQueue, incomingQueue);
    serialHandler->start();

    // Fire up the MessageProcessor
    auto messageProcessor = std::make_shared<creatures::MessageProcessor>(serialHandler);
    messageProcessor->start();


    // Create and start the e1.13 server
    debug("starting the e1.13 server");
    e131Server = std::make_shared<creatures::E131Server>();
    e131Server->start();

    // Create the servo controller
    //auto servoController = std::make_shared<I2CServoController>(i2cBus, PCA9685_I2C_ADDRESS);
    //servoController->begin();
    //trace("done with controller startup");

    //u8 current_pre_scale = servoController->readPrescale();
    //info("pre-scale is {}", current_pre_scale);



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

    info("the main thread says bye! good luck little threads! 👋🏻");
    return EXIT_SUCCESS;
}

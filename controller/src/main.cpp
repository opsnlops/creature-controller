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
#include "io/SerialOutput.h"


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

// Do we really want these in the global scope? 🤔
std::shared_ptr<creatures::MessageQueue<std::string>> outgoingQueue;
std::shared_ptr<creatures::MessageQueue<std::string>> incomingQueue;

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

    // Start up the SerialReader
    outgoingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    incomingQueue = std::make_shared<creatures::MessageQueue<std::string>>();
    auto SerialReader = std::make_shared<creatures::SerialOutput>("/dev/null", outgoingQueue, incomingQueue);
    SerialReader->start();



    std::shared_ptr<I2CDevice> i2cBus;

    // Create the correct i2c bus
    switch(config->getI2CBusType()) {

        case creatures::Configuration::I2CBusType::mock:
            i2cBus = std::make_shared<creatures::MockI2C>();
            break;

        default:
            critical("Unknown i2c bus type?");
            return EXIT_FAILURE;
    }



    // Start the i2c bus
    debug("starting the i2c bus");
    if(!i2cBus->start()) {
        critical("unable to open i2c device");
        return EXIT_FAILURE;
    }

    // Create and start the e1.13 server
    debug("starting the e1.13 server");
    e131Server = std::make_shared<creatures::E131Server>();
    e131Server->start();

    // Create the servo controller
    auto servoController = std::make_shared<I2CServoController>(i2cBus, PCA9685_I2C_ADDRESS);
    servoController->begin();
    trace("done with controller startup");

    u8 current_pre_scale = servoController->readPrescale();
    info("pre-scale is {}", current_pre_scale);



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

    info("testing sleep...");
    servoController->sleep();

    trace("sleeping for 2 more seconds");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    info("and now testing wakeup!");
    servoController->wakeup();

    trace("sleeping for 2 MORE MORE seconds");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    i2cBus->close();

    info("the main thread says bye! good luck little threads! 👋🏻");
    return EXIT_SUCCESS;
}

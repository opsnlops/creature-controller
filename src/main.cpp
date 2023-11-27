#include <chrono>
#include <cstdlib>
#include <locale>
#include <thread>

// This only exists on Linux
#ifdef __linux__
#include <bcm2835.h>
#endif


#include "controller-config.h"
#include "namespace-stuffs.h"
#include "pca9685/pca9685.h"

#include "config/command-line.h"
#include "config/config.h"
#include "device/i2c_mock.h"
#include "device/servo.h"
#include "dmx/e131_server.h"

#ifdef __linux__
#include "device/i2c_bcm2835.h"
#include "device/i2c_smbus.h"
#endif

// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"


#include "device/i2c_servo/i2c_servo.h"
#include "device/i2c_smbus.h"

// Declare the configuration globally
std::shared_ptr<creatures::Configuration> config;
std::shared_ptr<creatures::E131Server> e131Server;
std::shared_ptr<std::unordered_map<u8,Servo>> servos;
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

#ifdef __linux__
    debug("bcm2835 lib version {}", BCM2835_VERSION);
#endif

    // Parse out the command line options
    config = creatures::CommandLine::parseCommandLine(argc, argv);

#ifdef __linux__
    if (geteuid() != 0) {
        critical("This must be run as root!");
        return EXIT_FAILURE;
    }
#endif

    std::shared_ptr<I2CDevice> i2cBus;

    // Create the correct i2c bus
    switch(config->getI2CBusType()) {

        case creatures::Configuration::I2CBusType::mock:
            i2cBus = std::make_shared<creatures::MockI2C>();
            break;
#ifdef __linux__
        case creatures::Configuration::I2CBusType::bcm2835:
           i2cBus = std::make_shared<BCM2835I2C>();
           break;
        case creatures::Configuration::I2CBusType::smbus:
           i2cBus = std::make_shared<SMBusI2C>();
           break;
#endif
        default:
            critical("Unknown i2c bus type?");
            return EXIT_FAILURE;
    }

#ifdef __linux__
    // If this is an SMBus type, add the device node info
    if (auto smbusPtr = std::dynamic_pointer_cast<SMBusI2C>(i2cBus)) {
        smbusPtr->setDeviceNode(config->getSMBusDeviceNode());
        debug("set the device node to {}", config->getSMBusDeviceNode());
    }
#endif


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

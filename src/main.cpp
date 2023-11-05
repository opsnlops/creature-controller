#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <locale>
#include <cstring>

#include <thread>
#include <chrono>

#include <bcm2835.h>


#include "controller-config.h"
#include "namespace-stuffs.h"
#include "pca9685/pca9685.h"

// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"


#include "device/i2c_servo/i2c_servo.h"

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
    debug("bcm2835 lib version {}", BCM2835_VERSION);

    if (geteuid() != 0) {
        critical("This must be run as root!");
        return EXIT_FAILURE;
    }


    auto i2cBus = std::make_shared<BCM2835I2C>();
    i2cBus->start();

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
    servoController->setPWM(15, 0, 3000);

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

    // Clean up i2c
    bcm2835_i2c_end();
    debug("cleaned up i2c");

    // Stop the bcm2835 driver
    bcm2835_close();
    debug("stopped the bcm2835 driver");

    info("bye!");
    return EXIT_SUCCESS;
}

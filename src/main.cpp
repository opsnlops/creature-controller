#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <locale>
#include <cstring>

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

    // Start up the bcm2835 driver
    if (!bcm2835_init()) {
        critical("unable to start the bcm2835 driver");
        return EXIT_FAILURE;
    }
    debug("started the bcm2835 driver");


    // Tell the bcm2835 driver we wish to use i2c
    debug("opening i2c");
    if(!bcm2835_i2c_begin()) {
        error("unable to open i2c; are you root?");
        return EXIT_FAILURE;
    }

    // The datasheet says that the PCA9685 runs at 1Mhz max
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_626); // TODO: Can this go faster than 399.3610 kHz?


    auto servoController = std::make_shared<I2CServoController>(PCA9685_I2C_ADDRESS);
    servoController->begin();
    trace("done with controller startup");


    info("testing sleep...");
    servoController->sleep();

    info("and now testing wakeup!");
    servoController->wakeup();

    // Clean up i2c
    bcm2835_i2c_end();
    debug("cleaned up i2c");

    // Stop the bcm2835 driver
    bcm2835_close();
    debug("stopped the bcm2835 driver");

    info("bye!");
    return EXIT_SUCCESS;
}

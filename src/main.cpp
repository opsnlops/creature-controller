#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <locale>

#include <bcm2835.h>

#include "controller-config.h"
#include "namespace-stuffs.h"

// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main(int argc, char **argv) {

    try {
        // Set up our locale. If this vomits, install `locales-all`
        std::locale::global(std::locale("en_US.UTF-8"));
    }
    catch(const std::runtime_error& e) {
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

    if (!bcm2835_init())
        return EXIT_FAILURE;

    char buf[] = {0xE7};

    debug("{}", errno);

    bcm2835_i2c_begin();

    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_148);
    bcm2835_i2c_setSlaveAddress(0x40);

    bcm2835_i2c_write(buf, 1);
    bcm2835_i2c_read(buf, 1);

    debug("User Register = %X", buf[0]);

    bcm2835_i2c_end();

    return EXIT_SUCCESS;
}

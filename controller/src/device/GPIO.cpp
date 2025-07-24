
#include <chrono>
#include <fcntl.h>
#include <future>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

#include <fmt/format.h>

#include "logging/Logger.h"

#include "controller-config.h"

#include "GPIO.h"
#include "GPIOException.h"

namespace creatures::device {

GPIO::GPIO(const std::shared_ptr<creatures::Logger> &logger, bool enabled) : enabled(enabled), logger(logger) {}

void GPIO::init() {

    // If we're not enabled, bail out
    if (!enabled) {
        logger->info("GPIO not enabled 🚫");
        return;
    }

    int mem_fd;
    if ((mem_fd = open(GPIO_DEVICE, O_RDWR | O_SYNC)) < 0) {
        std::string errorMessage = fmt::format("Cannot open {}: {} (Hint, are we on a Raspberry Pi, "
                                               "and in the gpio group?)",
                                               GPIO_DEVICE, std::strerror(errno));
        logger->error(errorMessage);
        throw GPIOException(std::move(errorMessage));
    }

    // Memory map GPIO
    gpio_map = mmap(nullptr,                // Any address will do
                    GPIO_SIZE,              // Map length
                    PROT_READ | PROT_WRITE, // Enable reading & writing to mapped memory
                    MAP_SHARED,             // Shared with other processes
                    mem_fd,                 // File to map
                    GPIO_BASE               // Offset to GPIO peripheral
    );

    close(mem_fd); // We don't need the file descriptor after mmap

    if (gpio_map == MAP_FAILED) {
        logger->error("mmap error");
        return;
    }

    // Always use volatile pointer to hardware registers
    gpio = (volatile unsigned *)gpio_map;

    // Set up the pins as output pins
    set_output(gpio, FIRMWARE_RESET_PIN);

    // Default to the pins off
    turn_off(FIRMWARE_RESET_PIN);

    // Hooray!
    logger->info("GPIO enabled ✅");
}

void GPIO::toggleFirmwareReset() {
    if (enabled) {

        logger->debug("toggling the firmware reset pin");
        firmwareResetFuture = std::async(std::launch::async, [this]() {
            turn_on(FIRMWARE_RESET_PIN);
            logger->debug("firmware reset pin on, sleeping for 500ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            turn_off(FIRMWARE_RESET_PIN);
            logger->debug("firmware reset pin off");
        });
    }
}

void GPIO::set_output(volatile unsigned *gpio, int g) {
    *(gpio + ((g) / 10)) &= ~(7 << (((g) % 10) * 3));
    *(gpio + ((g) / 10)) |= (1 << (((g) % 10) * 3));
}

void GPIO::turn_on(int pin) { GPIO_SET = 1 << pin; }

void GPIO::turn_off(int pin) { GPIO_CLR = 1 << pin; }

} // namespace creatures::device
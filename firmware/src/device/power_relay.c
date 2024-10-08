

#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "device/power_relay.h"
#include "logging/logging.h"


#include "controller-config.h"
#include "types.h"

void init_power_relay() {

    // Set the GPIO pin to be an output pin
    gpio_init(POWER_PIN);
    gpio_set_dir(POWER_PIN, GPIO_OUT);

    // Default to off for safety
    power_relay_off();

    debug("power relay initialized");
}

void power_relay_on() {
    gpio_put(POWER_PIN, false);
    info("turned on the power relay");
}

void power_relay_off() {
    gpio_put(POWER_PIN, true);
    info("turned off the power relay");
}


#include <cstdlib>

#include "namespace-stuffs.h"

#include "relay.h"




/**
 * Creates a relay
 *
 * @param gpio_pin The GPIO pin the relay is on
 * @param on Should it be on by default?
 * @return a pointer to the Relay on the heap
 */
Relay::Relay(uint8_t gpio_pin, bool on) {

    debug("creating relay on gpio {}", gpio_pin);

    this->gpio_pin = gpio_pin;
    this->on = on;

    // TODO: Convert to Pi GPIOs
    //gpio_init(gpio_pin);
    //gpio_set_dir(gpio_pin, true);   // This is an output pin
    //gpio_put(gpio_pin, on);

}

bool Relay::isOn() const
{
    return on;
}


int Relay::turnOn() {

    debug("setting relay on GPIO {} to on", gpio_pin);

    // TODO: This
    //gpio_put(gpio_pin, true);
    on = true;

    return 0;
}

int Relay::turnOff() {

    debug("setting relay on GPIO {} to off", gpio_pin);

    // TODO: GPIOs
    //gpio_put(gpio_pin, false);
    on = false;

    return 0;
}


int Relay::toggle() {

    debug("toggling relay on GPIO {}", gpio_pin);

    if(on) {
        turnOff();
    } else {
        turnOn();
    }

    return 0;
}

#pragma once

#include <cstdio>

#include "controller-config.h"


/**
 * This is one relay in the system
 */
class Relay {

public:
    Relay(u8 gpio_pin, bool on);
    int turnOn();
    int turnOff();
    int toggle();

    bool isOn() const;

private:
    u8 gpio_pin;
    bool on;
};


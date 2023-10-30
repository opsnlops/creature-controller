#pragma once

#include <cstdio>

#include "controller-config.h"

#include "device/i2c_device.h"

class I2CServoController : public I2CDevice {

public:
    I2CServoController(u8 i2cAddress);
    void reset();
    void sleep();
    void wakeup();
    u8 begin();

    void setPWMFrequency(float frequency);

    void setOscillatorFrequency(u32 freq);
    u32 getOscillatorFrequency();

private:
    u32 _oscillator_freq;

};
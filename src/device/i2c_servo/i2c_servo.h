#pragma once

#include <cstdio>
#include <mutex>

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
    u32 getOscillatorFrequency() const;

    u8 readPrescale();

    u16 getPWM(u8 num, bool off);
    u8 setPWM(u8 num, u16 on, u16 off);
    void setPin(u8 num, u16 val, bool invert);


private:
    u32 _oscillator_freq;

    // Make sure only one thread can change our state at once
    std::mutex servo_mutex;

};
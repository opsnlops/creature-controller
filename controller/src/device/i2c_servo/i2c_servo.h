#pragma once

#include <cstdio>
#include <mutex>

#include "device/i2c.h"

#include "controller-config.h"

class I2CServoController {

public:
    I2CServoController(std::shared_ptr<I2CDevice> i2c, u8 i2cAddress);
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

    u8 getDeviceAddress() const;



private:
    u32 _oscillator_freq;

    std::shared_ptr<I2CDevice> i2c;

    // The address of this controller
    u8 i2cAddress;

    // Make sure only one thread can change our state at once
    std::mutex servo_mutex;

};
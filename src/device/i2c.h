
#pragma once

#include <mutex>
#include <string>

#include "controller-config.h"
#include "namespace-stuffs.h"


class I2CDevice {

public:

    I2CDevice() = default;
    virtual ~I2CDevice() = default;

    virtual u8 read8(u8 deviceAddress, u8 addr) const = 0;

    virtual void write8(u8 deviceAddress, u8 addr, u8 data) const = 0;
    virtual u8 write_then_read(u8 deviceAddress, char* commands, u32 commands_length, char* buffer, u32 buffer_length) const = 0;
    virtual u8 write(u8 deviceAddress, const char * buffer, u32 len) const = 0;

    // Give the device a chance to get running
    virtual u8 start() = 0;
    virtual u8 close() = 0;

    virtual std::string getDeviceType() = 0;

};

#pragma once

#include <mutex>

#include "controller-config.h"
#include "namespace-stuffs.h"


class I2CDevice {

public:
    u8 i2cAddress;

    u8 read8(uint8_t addr);
    void write8(u8 addr, u8 data);

    // Make sure only one thread can touch the bus at a time
    static std::mutex i2c_bus_mutex;

};
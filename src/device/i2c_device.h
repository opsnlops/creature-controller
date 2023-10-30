
#pragma once


#include "controller-config.h"
#include "namespace-stuffs.h"


class I2CDevice {

public:
    u8 i2cAddress;

    u8 read8(uint8_t addr);
    void write8(u8 addr, u8 data);

};
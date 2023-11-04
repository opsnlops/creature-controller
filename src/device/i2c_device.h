
#pragma once

#include <mutex>

#include "controller-config.h"
#include "namespace-stuffs.h"


class I2CDevice {

public:
    u8 i2cAddress;

    u8 read8(uint8_t addr);
    void write8(u8 addr, u8 data);
    u8 write_then_read(char* commands, u32 commands_length, char* buffer, u32 buffer_length);

    u8 write(const char * buffer, u32 len);

    // Make sure only one thread can touch the bus at a time
    static std::mutex i2c_bus_mutex;

};
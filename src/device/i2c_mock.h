
#pragma once

#include <mutex>

#include "controller-config.h"
#include "namespace-stuffs.h"
#include "i2c.h"


class SMBusI2C : public I2CDevice {

public:

    SMBusI2C();
    void setDeviceNode(std::string deviceNode);

    [[nodiscard]] u8 read8(u8 deviceAddress, u8 addr) override;
    void write8(u8 deviceAddress,  u8 addr, u8 data) override;
    u8 write_then_read(u8 deviceAddress, char* commands, u32 commands_length, char* buffer, u32 buffer_length) override;

    u8 write(u8 deviceAddress, const char * buffer, u32 len) override;

    u8 start() override;
    u8 close() override;

    std::string getDeviceType() override;

private:

    // Make sure only one thread can touch the bus at a time
    static std::mutex i2c_bus_mutex;

    std::string deviceNode;
    int fd;

    void ensureDevice(u8 deviceAddress);
    u8 currentDeviceAddress;

};
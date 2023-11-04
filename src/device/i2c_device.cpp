
#include "controller-config.h"
#include "namespace-stuffs.h"

#include <mutex>

#include <bcm2835.h>


#include "i2c_device.h"


// Initialize the mutex
std::mutex I2CDevice::i2c_bus_mutex;

void I2CDevice::write8(u8 addr, u8 data) {

    debug("write8 called with address 0x{0:x}, data 0x{0:x}", addr, data);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    char buffer[] = {addr, data};
    bcm2835_i2c_setSlaveAddress(this->i2cAddress);
    u8 returnVal = bcm2835_i2c_write(buffer, 2);

    debug("write() return value was {}", returnVal);

}


u8 I2CDevice::read8(uint8_t addr) {

    debug("read8 called for address {}", addr);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    char buffer[] = {addr};
    bcm2835_i2c_setSlaveAddress(this->i2cAddress);
    bcm2835_i2c_read_register_rs(buffer, buffer, 1);

    u8 data = (u8)buffer[0];

    trace("we read data 0x{0:x}", data);
    return data;
}


u8 I2CDevice::write_then_read(char* commands, u32 commands_length, char* buffer, u32 buffer_length) {

    debug("write_then_read called with command length {} and buffer length {}", commands_length, buffer_length);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    bcm2835_i2c_setSlaveAddress(this->i2cAddress);
    u8 result = bcm2835_i2c_write_read_rs(commands, commands_length, buffer, buffer_length);

    debug("bcm2835_i2c_write_read_rs result was {}", result);

    return result;
}

u8 I2CDevice::write(const char * buffer, u32 len) {

    debug("writing a buffer of {} length", len);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    bcm2835_i2c_setSlaveAddress(this->i2cAddress);

    u8 result = bcm2835_i2c_write(buffer, len);
    debug("write result was {}");

    return result;

}
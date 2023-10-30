
#include "controller-config.h"
#include "namespace-stuffs.h"

#include <bcm2835.h>

#include "i2c_device.h"

void I2CDevice::write8(u8 addr, u8 data) {

    trace("write8 called with address {}, data {}", addr, data);

    char buffer[] = {addr, data};
    bcm2835_i2c_setSlaveAddress(this->i2cAddress);
    u8 returnVal = bcm2835_i2c_write(buffer, 2);

    trace("write() return value was {}", returnVal);

}


u8 I2CDevice::read8(uint8_t addr) {

    trace("read8 called for address {}", addr);

    char buffer[] = {addr};
    bcm2835_i2c_setSlaveAddress(this->i2cAddress);
    bcm2835_i2c_read_register_rs(buffer, buffer, 1);

    u8 data = (u8)buffer[0];

    trace("we read data 0x{0:x}", data);
    return data;
}

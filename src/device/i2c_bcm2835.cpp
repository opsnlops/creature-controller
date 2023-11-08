
#include "controller-config.h"
#include "namespace-stuffs.h"

#include <mutex>

#include <bcm2835.h>

#include "i2c_bcm2835.h"


// Initialize the mutex
std::mutex BCM2835I2C::i2c_bus_mutex;

BCM2835I2C::BCM2835I2C() : I2CDevice() {

    debug("new BCM2835I2C made");
}

std::string BCM2835I2C::getDeviceType() {
    return "bcm2835";
}

u8 BCM2835I2C::start() {

    debug("starting up BCM2835I2C!");

    // Start up the bcm2835 driver
    if (!bcm2835_init()) {
        critical("unable to start the bcm2835 driver");
        return 0;
    }
    debug("started the bcm2835 driver");


    // Tell the bcm2835 driver we wish to use i2c
    debug("opening i2c");
    if(!bcm2835_i2c_begin()) {
        error("unable to open i2c; are you root?");
        return 0;
    }

    // The datasheet says that the PCA9685 runs at 1Mhz max
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_626); // TODO: Can this go faster than 399.3610 kHz?

    return 1;

}

void BCM2835I2C::write8(u8 deviceAddress, u8 addr, u8 data) {

    debug("write8 called with address 0x{0:x}, data 0x{0:x}", addr, data);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    u8 buffer[] = {addr, data};
    bcm2835_i2c_setSlaveAddress(deviceAddress);
    u8 returnVal = bcm2835_i2c_write(reinterpret_cast<const char *>(buffer), 2);

    debug("write() return value was {}", returnVal);

}


u8 BCM2835I2C::read8(u8 deviceAddress, u8 addr) {

    debug("read8 called for address {}", addr);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    u8 buffer[] = {addr};
    bcm2835_i2c_setSlaveAddress(deviceAddress);
    bcm2835_i2c_read_register_rs(reinterpret_cast<char *>(buffer), reinterpret_cast<char *>(buffer), 1);

    u8 data = (u8)buffer[0];

    trace("we read data 0x{0:x}", data);
    return data;
}


u8 BCM2835I2C::write_then_read(u8 deviceAddress, char* commands, u32 commands_length, char* buffer, u32 buffer_length) {

    debug("write_then_read called with command length {} and buffer length {}", commands_length, buffer_length);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    bcm2835_i2c_setSlaveAddress(deviceAddress);
    u8 result = bcm2835_i2c_write_read_rs(commands, commands_length, buffer, buffer_length);

    debug("bcm2835_i2c_write_read_rs result was {}", result);

    return result;
}

u8 BCM2835I2C::write(u8 deviceAddress, const char * buffer, u32 len) {

    debug("writing a buffer of {} length", len);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    bcm2835_i2c_setSlaveAddress(deviceAddress);

    u8 result = bcm2835_i2c_write(buffer, len);
    debug("write result was {}");

    return result;

}

u8 BCM2835I2C::close() {

    info("shutting down the bcm2835 i2c bus");

    // Clean up i2c
    bcm2835_i2c_end();
    debug("cleaned up i2c");

    // Stop the bcm2835 driver
    bcm2835_close();
    debug("stopped the bcm2835 driver");

    return 1;

}

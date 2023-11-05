
#include "controller-config.h"
#include "namespace-stuffs.h"

#include <filesystem>


#include "i2c_mock.h"

namespace creatures {

// Initialize the mutex
    std::mutex MockI2C::i2c_bus_mutex;

    MockI2C::MockI2C() : I2CDevice() {

        debug("new MockI2C made");
    }

    std::string MockI2C::getDeviceType() {
        return "mock";
    }


    u8 MockI2C::start() {

        debug("starting up MockI2C!");
        return 1;
    }

    void MockI2C::write8(u8 deviceAddress, u8 addr, u8 data) {

        debug("write8 called with address 0x{0:x}, data 0x{0:x}", addr, data);

        std::lock_guard<std::mutex> lock(i2c_bus_mutex);
        trace("i2c mutex acquired");
    }


    u8 MockI2C::read8(u8 deviceAddress, u8 addr) {

        debug("read8 called for address {}", addr);

        std::lock_guard<std::mutex> lock(i2c_bus_mutex);
        trace("i2c mutex acquired");

        return 1;
    }


    u8
    MockI2C::write_then_read(u8 deviceAddress, char *commands, u32 commands_length, char *buffer, u32 buffer_length) {

        debug("write_then_read called with command length {} and buffer length {}", commands_length, buffer_length);

        std::lock_guard<std::mutex> lock(i2c_bus_mutex);
        trace("i2c mutex acquired");

        return 1;
    }

    u8 MockI2C::write(u8 deviceAddress, const char *buffer, u32 len) {

        debug("writing a buffer of {} length", len);

        std::lock_guard<std::mutex> lock(i2c_bus_mutex);
        trace("i2c mutex acquired");

        return 1;
    }


    u8 MockI2C::close() {

        info("shutting down the MockI2C i2c bus");
        return 1;
    }

}
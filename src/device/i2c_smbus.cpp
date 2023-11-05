
#include "controller-config.h"
#include "namespace-stuffs.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <filesystem>

extern "C"
{
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
}


#include "i2c_smbus.h"


// Initialize the mutex
std::mutex SMBusI2C::i2c_bus_mutex;

SMBusI2C::SMBusI2C() : I2CDevice() {

    debug("new SMBusI2C made");
    fd = 255;
    currentDeviceAddress = 255;
}

std::string SMBusI2C::getDeviceType() {
    return "smbus";
}

void SMBusI2C::setDeviceNode(std::string _deviceNode) {
    debug("setting the device node to {}", _deviceNode);

    this->deviceNode = _deviceNode;

}

u8 SMBusI2C::start() {

    debug("starting up SMBusI2C!");

    // Make sure that the device exists
    if (!std::filesystem::exists(deviceNode)) {
        error("unable to open i2c-dev device at {} because it doesn't exist ({})", deviceNode, errno);
        return 0;
    }
    debug("confirmed that {} exists", deviceNode);

    // Open the I2C device
    fd = open(deviceNode.c_str(), O_RDWR);
    if (fd < 0) {
        error("unable to open {} ({})", deviceNode, errno);
        return 0;
    }
    debug("opened {}! File descriptor is {}.", deviceNode, fd);

    // Try to get the supported features at a smoke test
    u32 funcs;
    if( ioctl(fd, I2C_FUNCS, &funcs) < 0) {
        error("unable to determine the adaptor's features ({})", errno);
        return 0;
    }
    debug("supported functions: {}", funcs);

    // Set this to a junk value that can't actually exist
    currentDeviceAddress = 255;

    return 1;

}

void SMBusI2C::write8(u8 deviceAddress, u8 addr, u8 data) {

    debug("write8 called with address 0x{0:x}, data 0x{0:x}", addr, data);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    ensureDevice(deviceAddress);

    u8 returnVal = i2c_smbus_write_byte_data(fd, addr, data);

    debug("write() return value was {}", returnVal);

}


u8 SMBusI2C::read8(u8 deviceAddress, u8 addr) {

    debug("read8 called for address {}", addr);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    ensureDevice(deviceAddress);

    u8 data = i2c_smbus_read_byte_data(fd, addr);

    trace("we read data 0x{0:x}", data);
    return data;
}


u8 SMBusI2C::write_then_read(u8 deviceAddress, char* commands, u32 commands_length, char* buffer, u32 buffer_length) {

    debug("write_then_read called with command length {} and buffer length {}", commands_length, buffer_length);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    // TODO: This isn't pretty, but it's not called much

    ::write(fd, commands, commands_length);
    u8 result = ::read(fd, buffer, buffer_length);

    debug("read result was {}", result);

    return result;
}

u8 SMBusI2C::write(u8 deviceAddress, const char * buffer, u32 len) {

    debug("writing a buffer of {} length", len);

    std::lock_guard<std::mutex> lock(i2c_bus_mutex);
    trace("i2c mutex acquired");

    ensureDevice(deviceAddress);

    u8 result = ::write(fd, buffer, len);
    //u8 result = i2c_smbus_write_block_data(fd, command, len, reinterpret_cast<const __u8 *>(buffer));

    debug("write result was {}", result);

    return result;

}

void SMBusI2C::ensureDevice(u8 deviceAddress) {

    trace("making sure we're talking to device 0x{0:x}", deviceAddress);

    if(currentDeviceAddress != deviceAddress) {
        debug("changing device to 0x{0:x}", deviceAddress);
        if( ioctl(fd, I2C_SLAVE, deviceAddress) < 0) {
            error("unable to change i2c device address");
        }
        currentDeviceAddress = deviceAddress;
    }
}

u8 SMBusI2C::close() {

    info("shutting down the SMBus i2c bus");
    ::close(fd);

    return 1;

}
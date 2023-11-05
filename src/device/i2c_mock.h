
#pragma once

#include <mutex>

#include "controller-config.h"
#include "namespace-stuffs.h"
#include "i2c.h"

namespace creatures {

    /**
     * This literally does nothing other than log to the system logger. It's to allow
     * working on the controller when there's not really a need to send i2c messages.
     */
    class MockI2C : public I2CDevice {

    public:

        MockI2C();

        [[nodiscard]] u8 read8(u8 deviceAddress, u8 addr) override;

        void write8(u8 deviceAddress, u8 addr, u8 data) override;

        u8 write_then_read(u8 deviceAddress, char *commands, u32 commands_length, char *buffer,
                           u32 buffer_length) override;

        u8 write(u8 deviceAddress, const char *buffer, u32 len) override;

        u8 start() override;

        u8 close() override;

        std::string getDeviceType() override;

    private:

        // Make sure only one thread can touch the bus at a time
        static std::mutex i2c_bus_mutex;

    };
}
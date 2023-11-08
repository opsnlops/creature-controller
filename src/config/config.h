#pragma once

namespace creatures {

    class Configuration {

    public:

        Configuration();

        friend class CommandLine;

        // Define the bus type we're using
        enum class I2CBusType { mock, smbus, bcm2835 };


        [[nodiscard]] I2CBusType getI2CBusType() const;
        [[nodiscard]] std::string getSMBusDeviceNode() const;

    protected:
        void setI2CBusType(Configuration::I2CBusType _i2CBusType);
        void setSMBusDeviceNode(std::string _smbusDeviceNode);

    private:
        I2CBusType i2CBusType;

        // If we're using smbus, which device node?
        std::string smbusDeviceNode;

    };


}
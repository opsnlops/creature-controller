#pragma once

namespace creatures {

    class Configuration {

    public:

        Configuration();

        friend class CommandLine;

        // Define the bus type we're using
        enum class I2CBusType { mock, smbus };


        [[nodiscard]] I2CBusType getI2CBusType() const;
        [[nodiscard]] std::string getSMBusDeviceNode() const;
        std::string getConfigFileName() const;

    protected:
        void setI2CBusType(Configuration::I2CBusType _i2CBusType);
        void setSMBusDeviceNode(std::string _smbusDeviceNode);
        void setConfigFileName(std::string _configFileName);

    private:
        I2CBusType i2CBusType;

        // If we're using smbus, which device node?
        std::string smbusDeviceNode;

        // The location of our JSON config file
        std::string configFileName;

    };


}
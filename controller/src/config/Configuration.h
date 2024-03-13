#pragma once

#include <string>
#include <vector>

#include "config/UARTDevice.h"

#include "controller-config.h"
#include "logging/Logger.h"

namespace creatures {

    class Configuration {

    public:

        Configuration(std::shared_ptr<Logger> logger);

        friend class CommandLine;

        std::string getConfigFileName() const;
        std::string getUsbDevice() const;
        bool getUseGPIO() const;
        std::string getNetworkDeviceIPAddress() const;
        std::vector<UARTDevice> getUARTDevices() const;

    protected:
        void setConfigFileName(std::string _configFileName);
        void setUsbDevice(std::string _usbDevice);
        void setUseGPIO(bool _useGPIO);
        void setNetworkDeviceIPAddress(std::string _networkDeviceIPAddress);
        void addUARTDevice(UARTDevice _uartDevice);

    private:

        // The device node of our USB device
        std::string usbDevice;

        // The location of our JSON config file
        std::string configFileName;

        std::shared_ptr<Logger> logger;

        // Should we use the GPIO pins?
        bool useGPIO = false;

        // Network stuff
        std::string networkDeviceIPAddress = DEFAULT_NETWORK_DEVICE_IP_ADDRESS;

        // UARTs
        std::vector<UARTDevice> uartDevices;
    };

}
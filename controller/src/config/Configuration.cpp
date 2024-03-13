
#include <string>
#include <vector>
#include <utility>

#include "logging/Logger.h"

#include "config/Configuration.h"



namespace creatures {

    Configuration::Configuration(std::shared_ptr<Logger> logger) : logger(logger) {
        logger->debug("creating a new Configuration");
    }

    std::string Configuration::getConfigFileName() const {
        return this->configFileName;
    }

    std::string Configuration::getUsbDevice() const {
        return this->usbDevice;
    }

    bool Configuration::getUseGPIO() const {
        return this->useGPIO;
    }

    std::vector<UARTDevice> Configuration::getUARTDevices() const {
        return this->uartDevices;
    }


    void Configuration::setConfigFileName(std::string _configFileName) {
        this->configFileName = std::move(_configFileName);

        logger->debug("set configFileName to {}", this->configFileName);
    }

    void Configuration::setUsbDevice(std::string _usbDevice) {
        this->usbDevice = std::move(_usbDevice);
    }

    void Configuration::setUseGPIO(bool _useGPIO) {
        this->useGPIO = _useGPIO;
    }

    std::string Configuration::getNetworkDeviceIPAddress() const {
        return this->networkDeviceIPAddress;
    }

    void Configuration::setNetworkDeviceIPAddress(std::string _networkDeviceIPAddress) {
        this->networkDeviceIPAddress = _networkDeviceIPAddress;
    }

    void Configuration::addUARTDevice(UARTDevice _uartDevice) {
        this->uartDevices.push_back(_uartDevice);
    }
}
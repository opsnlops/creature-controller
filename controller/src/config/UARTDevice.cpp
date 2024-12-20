
#include "logging/Logger.h"

#include "config/UARTDevice.h"

namespace creatures :: config {

    UARTDevice::UARTDevice(std::shared_ptr<Logger> logger) : logger(logger) {
        logger->debug("creating a new UARTDevice");
    }

    UARTDevice::~UARTDevice() {
        logger->debug("destroyed a UARTDevice");
    }

    UARTDevice::UARTDevice(const UARTDevice &other) {
        this->logger = other.logger;
        this->enabled = other.enabled;
        this->deviceNode = other.deviceNode;
        this->module = other.module;
    }

    UARTDevice::module_name UARTDevice::stringToModuleName(const std::string &typeStr) {
        if (typeStr == "A") return A;
        if (typeStr == "B") return B;
        if (typeStr == "C") return C;
        if (typeStr == "D") return D;
        if (typeStr == "E") return E;
        if (typeStr == "F") return F;
        return invalid_module;
    }

    std::string UARTDevice::getDeviceNode() const {
        return this->deviceNode;
    }

    UARTDevice::module_name UARTDevice::getModule() const {
        return this->module;
    }

    bool UARTDevice::getEnabled() const {
        return this->enabled;
    }

    void UARTDevice::setDeviceNode(std::string _deviceNode) {
        this->deviceNode = _deviceNode;
    }

    void UARTDevice::setModule(module_name _module) {
        this->module = _module;
    }

    void UARTDevice::setEnabled(bool _enabled) {
        this->enabled = _enabled;
    }

} // creatures :: config

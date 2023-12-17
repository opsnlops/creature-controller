
#include "controller-config.h"
#include "namespace-stuffs.h"

#include "config.h"

#include <utility>

namespace creatures {

    Configuration::Configuration() {
        debug("creating a new Configuration");

    }

    std::string Configuration::getConfigFileName() const {
        return this->configFileName;
    }

    std::string Configuration::getUsbDevice() const {
        return this->usbDevice;
    }

    void Configuration::setConfigFileName(std::string _configFileName) {
        this->configFileName = std::move(_configFileName);

        debug("set configFileName to {}", this->configFileName);
    }

    void Configuration::setUsbDevice(std::string _usbDevice) {
        this->usbDevice = std::move(_usbDevice);
    }
}
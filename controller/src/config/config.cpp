
#include "controller-config.h"
#include "namespace-stuffs.h"

#include "config.h"

#include <utility>

namespace creatures {

    Configuration::Configuration() {
        debug("creating a new Configuration");

        this->i2CBusType = Configuration::I2CBusType::mock;
        this->smbusDeviceNode = "";
    }

    Configuration::I2CBusType Configuration::getI2CBusType() const {
        return this->i2CBusType;
    }

    void Configuration::setI2CBusType(Configuration::I2CBusType _i2CBusType) {
        this->i2CBusType = _i2CBusType;

        debug("set the i2c bus type");
    }


    std::string Configuration::getSMBusDeviceNode() const {
        return this->smbusDeviceNode;
    }

    void Configuration::setSMBusDeviceNode(std::string _smbusDeviceNode) {
        this->smbusDeviceNode = std::move(_smbusDeviceNode);

        debug("set smbBusNode to {}", this->smbusDeviceNode);
    }

    std::string Configuration::getConfigFileName() const {
        return this->configFileName;
    }

    void Configuration::setConfigFileName(std::string _configFileName) {
        this->configFileName = std::move(_configFileName);

        debug("set configFileName to {}", this->configFileName);
    }

}

#include <string>
#include <vector>
#include <utility>

#include "logging/Logger.h"

#include "config/Configuration.h"



namespace creatures :: config {

    Configuration::Configuration(std::shared_ptr<Logger> logger) : logger(logger) {
        logger->debug("creating a new Configuration");
    }

    bool Configuration::getUseGPIO() const {
        return this->useGPIO;
    }

    std::string Configuration::getNetworkDeviceIPAddress() const {
        return this->networkDeviceIPAddress;
    }

    u16 Configuration::getUniverse() const {
        return this->universe;
    }

    std::vector<UARTDevice> Configuration::getUARTDevices() const {
        return this->uartDevices;
    }

    std::shared_ptr<creatures::creature::Creature> Configuration::getCreature() const {
        return this->creature;
    }

    std::string Configuration::getCreatureConfigFile() const {
        return this->creatureConfigFile;
    }




    void Configuration::setUseGPIO(bool _useGPIO) {
        this->useGPIO = _useGPIO;
        logger->debug("set useGPIO to {}", this->useGPIO);
    }

    void Configuration::setNetworkDeviceIPAddress(std::string _networkDeviceIPAddress) {
        this->networkDeviceIPAddress = _networkDeviceIPAddress;
        logger->debug("set networkDeviceIPAddress to {}", this->networkDeviceIPAddress);
    }

    void Configuration::setUniverse(u16 _universe) {
        this->universe = _universe;
        logger->debug("set universe to {}", this->universe);
    }

    void Configuration::addUARTDevice(UARTDevice _uartDevice) {
        this->uartDevices.push_back(_uartDevice);
    }

    void Configuration::setCreature(std::shared_ptr<creatures::creature::Creature> _creature) {
        this->creature = _creature;
    }

    void Configuration::setCreatureConfigFile(std::string _creatureConfigFile) {
        this->creatureConfigFile = _creatureConfigFile;
    }

} // creatures :: config
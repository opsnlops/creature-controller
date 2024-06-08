#pragma once

#include <string>
#include <vector>

#include "config/UARTDevice.h"
#include "creature/Creature.h"

#include "controller-config.h"
#include "logging/Logger.h"

using namespace creatures;
using namespace creatures::config;

namespace creatures::config {

    class Configuration {

    public:

        Configuration(std::shared_ptr<Logger> logger);
        ~Configuration() = default;

        friend class ConfigurationBuilder;
        friend class CommandLine;
        friend class Configuration;

        bool getUseGPIO() const;
        std::string getNetworkDeviceIPAddress() const;
        u16 getUniverse() const;
        [[nodiscard]] std::vector<UARTDevice> getUARTDevices() const;
        std::shared_ptr<creatures::creature::Creature> getCreature() const;
        std::string getCreatureConfigFile() const;

        void setUseGPIO(bool _useGPIO);
        void setNetworkDeviceIPAddress(std::string _networkDeviceIPAddress);
        void setUniverse(u16 _universe);
        void addUARTDevice(UARTDevice _uartDevice);
        void setCreature(std::shared_ptr<creatures::creature::Creature> _creature);
        void setCreatureConfigFile(std::string _creatureConfigFile);

    private:

        // Should we use the GPIO pins?
        bool useGPIO = false;

        // Which IP address to bind to?
        std::string networkDeviceIPAddress = DEFAULT_NETWORK_DEVICE_IP_ADDRESS;

        // Which e1.31 universe are we using?
        u16 universe = DEFAULT_UNIVERSE;

        // UARTs
        std::vector<UARTDevice> uartDevices;

        std::shared_ptr<Logger> logger;

        std::shared_ptr<creatures::creature::Creature> creature;

        std::string creatureConfigFile;

    };

} // creatures :: config

/**
 * @file Configuration.cpp
 * @brief Implementation of the Configuration class for controller settings
 */

// Standard library includes
#include <string>
#include <vector>
#include <utility>

// Project includes
#include "controller-config.h" // For u16 type definition
#include "logging/Logger.h"
#include "config/Configuration.h"
#include "config/UARTDevice.h"
#include "creature/Creature.h"

namespace creatures::config {

    /**
     * @brief Constructor for the Configuration class
     * @param logger Shared pointer to a logger instance
     * @throws std::invalid_argument if logger is null
     */
    Configuration::Configuration(std::shared_ptr<Logger> logger)
        : universe(1),
          logger(std::move(logger)),
          serverPort(8080) {
        
        if (!this->logger) {
            throw std::invalid_argument("Logger cannot be null");
        }
        
        this->logger->debug("Creating a new Configuration");
    }

    //----------------------------------------------------------------------------
    // Getter Methods
    //----------------------------------------------------------------------------

    /**
     * @brief Check if GPIO pins should be used
     * @return True if GPIO should be used, false otherwise
     */
    bool Configuration::getUseGPIO() const {
        return useGPIO;
    }

    /**
     * @brief Get the IP address to bind to for network communication
     * @return The network device IP address as a string
     */
    std::string Configuration::getNetworkDeviceIPAddress() const {
        return networkDeviceIPAddress;
    }

    /**
     * @brief Get the E1.31 universe to use
     * @return The universe ID
     */
    u16 Configuration::getUniverse() const {
        return universe;
    }

    /**
     * @brief Get the configured UART devices
     * @return Vector of UARTDevice instances
     */
    std::vector<UARTDevice> Configuration::getUARTDevices() const {
        return uartDevices;
    }

    /**
     * @brief Get the configured creature
     * @return Shared pointer to the creature instance
     */
    std::shared_ptr<creatures::creature::Creature> Configuration::getCreature() const {
        return creature;
    }

    /**
     * @brief Get the path to the creature configuration file
     * @return Path to the creature configuration file
     */
    std::string Configuration::getCreatureConfigFile() const {
        return creatureConfigFile;
    }

    /**
     * @brief Check if the controller should connect to a server
     * @return True if a server connection should be used, false otherwise
     */
    bool Configuration::isUsingServer() const {
        return useServer;
    }

    /**
     * @brief Get the server address to connect to
     * @return The server address as a string
     */
    std::string Configuration::getServerAddress() const {
        return serverAddress;
    }

    /**
     * @brief Get the server port to connect to
     * @return The server port number
     */
    u16 Configuration::getServerPort() const {
        return serverPort;
    }

    //----------------------------------------------------------------------------
    // Setter Methods
    //----------------------------------------------------------------------------

    /**
     * @brief Set whether to use GPIO pins
     * @param _useGPIO True if GPIO should be used, false otherwise
     */
    void Configuration::setUseGPIO(bool _useGPIO) {
        this->useGPIO = _useGPIO;
        logger->debug("Set useGPIO to {}", this->useGPIO);
    }

    /**
     * @brief Set the IP address to bind to for network communication
     * @param _networkDeviceIPAddress The network device IP address
     */
    void Configuration::setNetworkDeviceIPAddress(std::string _networkDeviceIPAddress) {
        this->networkDeviceIPAddress = std::move(_networkDeviceIPAddress);
        logger->debug("Set networkDeviceIPAddress to {}", this->networkDeviceIPAddress);
    }

    /**
     * @brief Set the E1.31 universe to use
     * @param _universe The universe ID
     */
    void Configuration::setUniverse(u16 _universe) {
        this->universe = _universe;
        logger->debug("Set universe to {}", this->universe);
    }

    /**
     * @brief Add a UART device to the configuration
     * @param _uartDevice The UART device to add
     */
    void Configuration::addUARTDevice(UARTDevice _uartDevice) {
        uartDevices.push_back(std::move(_uartDevice));
        logger->debug("Added UART device with module {}",
                     static_cast<int>(uartDevices.back().getModule()));
    }

    /**
     * @brief Set the creature instance
     * @param _creature Shared pointer to the creature instance
     */
    void Configuration::setCreature(std::shared_ptr<creatures::creature::Creature> _creature) {
        this->creature = std::move(_creature);
        logger->debug("Set creature");
    }

    /**
     * @brief Set the path to the creature configuration file
     * @param _creatureConfigFile Path to the creature configuration file
     */
    void Configuration::setCreatureConfigFile(std::string _creatureConfigFile) {
        this->creatureConfigFile = std::move(_creatureConfigFile);
        logger->debug("Set creature config file to {}", this->creatureConfigFile);
    }

    /**
     * @brief Set whether the controller should connect to a server
     * @param _useServer True if a server connection should be used, false otherwise
     */
    void Configuration::setUseServer(bool _useServer) {
        this->useServer = _useServer;
        logger->debug("Set useServer to {}", this->useServer);
    }

    /**
     * @brief Set the server address to connect to
     * @param _serverAddress The server address
     */
    void Configuration::setServerAddress(std::string _serverAddress) {
        this->serverAddress = std::move(_serverAddress);
        logger->debug("Set server address to {}", this->serverAddress);
    }

    /**
     * @brief Set the server port to connect to
     * @param _serverPort The server port number
     */
    void Configuration::setServerPort(u16 _serverPort) {
        this->serverPort = _serverPort;
        logger->debug("Set server port to {}", this->serverPort);
    }

} // namespace creatures::config
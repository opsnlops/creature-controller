
/**
 * @file Configuration.cpp
 * @brief Implementation of the Configuration class for controller settings
 */

// Standard library includes
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <string>
#include <utility>
#include <vector>

// Project includes
#include "config/Configuration.h"
#include "config/UARTDevice.h"
#include "controller-config.h" // For u16 type definition
#include "creature/Creature.h"
#include "logging/Logger.h"

namespace creatures::config {

/**
 * @brief Constructor for the Configuration class
 * @param logger Shared pointer to a logger instance
 * @throws std::invalid_argument if logger is null
 */
Configuration::Configuration(std::shared_ptr<Logger> logger)
    : universe(1), logger(std::move(logger)), serverPort(8080) {

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
bool Configuration::getUseGPIO() const { return useGPIO; }

bool Configuration::getUseAudioSubsystem() const { return useAudioSubsystem; }

u8 Configuration::getSoundDeviceNumber() const { return soundDeviceNumber; }

uint Configuration::getNetworkDeviceIndex() const { return networkDeviceIndex; }

std::string Configuration::getNetworkDeviceName() { return networkDeviceName; }

/**
 * @brief Get the IP address to bind to for network communication
 * @return The network device IP address as a string
 */
std::string Configuration::getNetworkDeviceIPAddress() const { return networkDeviceIPAddress; }

/**
 * @brief Get the E1.31 universe to use
 * @return The universe ID
 */
u16 Configuration::getUniverse() const { return universe; }

/**
 * @brief Get the configured UART devices
 * @return Vector of UARTDevice instances
 */
std::vector<UARTDevice> Configuration::getUARTDevices() const { return uartDevices; }

/**
 * @brief Get the configured creature
 * @return Shared pointer to the creature instance
 */
std::shared_ptr<creatures::creature::Creature> Configuration::getCreature() const { return creature; }

/**
 * @brief Get the path to the creature configuration file
 * @return Path to the creature configuration file
 */
std::string Configuration::getCreatureConfigFile() const { return creatureConfigFile; }

/**
 * @brief Check if the controller should connect to a server
 * @return True if a server connection should be used, false otherwise
 */
bool Configuration::isUsingServer() const { return useServer; }

/**
 * @brief Get the server address to connect to
 * @return The server address as a string
 */
std::string Configuration::getServerAddress() const { return serverAddress; }

/**
 * @brief Get the server port to connect to
 * @return The server port number
 */
u16 Configuration::getServerPort() const { return serverPort; }

/**
 * @brief Get the power draw limit in watts
 * @return The power draw limit in watts
 */
double Configuration::getPowerDrawLimitWatts() const { return powerDrawLimitWatts; }

/**
 * @brief Get the power draw warning threshold in watts
 * @return The power draw warning threshold in watts
 */
double Configuration::getPowerDrawWarningWatts() const { return powerDrawWarningWatts; }

/**
 * @brief Get the power draw response time in seconds
 * @return The power draw response time in seconds
 */
double Configuration::getPowerDrawResponseSeconds() const { return powerDrawResponseSeconds; }

/**
 * @brief Get the temperature limit in degrees
 * @return The temperature limit in degrees
 */
double Configuration::getTemperatureLimitDegrees() const { return temperatureLimitDegrees; }

/**
 * @brief Get the temperature warning threshold in degrees
 * @return The temperature warning threshold in degrees
 */
double Configuration::getTemperatureWarningDegrees() const { return temperatureWarningDegrees; }

/**
 * @brief Get the temperature limit response time in seconds
 * @return The temperature limit response time in seconds
 */
double Configuration::getTemperatureLimitSeconds() const { return temperatureLimitSeconds; }

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

void Configuration::setSoundDeviceNumber(u8 _soundDeviceNumber) {
    this->soundDeviceNumber = _soundDeviceNumber;
    logger->debug("Set soundDeviceNumber to {}", this->soundDeviceNumber);
}

void Configuration::setUseAudioSubsystem(bool _useAudioSubsystem) {
    this->useAudioSubsystem = _useAudioSubsystem;
    logger->debug("Set useAudioSubsystem to {}", this->useAudioSubsystem);
}

/**
 * @brief Set the network interface to use
 * @param _networkDeviceName The network interface (eg eth0)
 */
void Configuration::setNetworkDeviceName(const std::string &_networkDeviceName) {
    this->networkDeviceName = _networkDeviceName;
    logger->debug("Set networkDeviceName to {}", this->networkDeviceName);
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
    logger->debug("Added UART device with module {}", static_cast<int>(uartDevices.back().getModule()));
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

/**
 * @brief Set the power draw limit in watts
 * @param _powerDrawLimitWatts The power draw limit in watts
 */
void Configuration::setPowerDrawLimitWatts(double _powerDrawLimitWatts) {
    this->powerDrawLimitWatts = _powerDrawLimitWatts;
    logger->debug("Set power draw limit to {} watts", this->powerDrawLimitWatts);
}

/**
 * @brief Set the power draw warning threshold in watts
 * @param _powerDrawWarningWatts The power draw warning threshold in watts
 */
void Configuration::setPowerDrawWarningWatts(double _powerDrawWarningWatts) {
    this->powerDrawWarningWatts = _powerDrawWarningWatts;
    logger->debug("Set power draw warning to {} watts", this->powerDrawWarningWatts);
}

/**
 * @brief Set the power draw response time in seconds
 * @param _powerDrawResponseSeconds The power draw response time in seconds
 */
void Configuration::setPowerDrawResponseSeconds(double _powerDrawResponseSeconds) {
    this->powerDrawResponseSeconds = _powerDrawResponseSeconds;
    logger->debug("Set power draw response time to {} seconds", this->powerDrawResponseSeconds);
}

/**
 * @brief Set the temperature limit in degrees
 * @param _temperatureLimitDegrees The temperature limit in degrees
 */
void Configuration::setTemperatureLimitDegrees(double _temperatureLimitDegrees) {
    this->temperatureLimitDegrees = _temperatureLimitDegrees;
    logger->debug("Set temperature limit to {} degrees", this->temperatureLimitDegrees);
}

/**
 * @brief Set the temperature warning threshold in degrees
 * @param _temperatureWarningDegrees The temperature warning threshold in degrees
 */
void Configuration::setTemperatureWarningDegrees(double _temperatureWarningDegrees) {
    this->temperatureWarningDegrees = _temperatureWarningDegrees;
    logger->debug("Set temperature warning to {} degrees", this->temperatureWarningDegrees);
}

/**
 * @brief Set the temperature limit response time in seconds
 * @param _temperatureLimitSeconds The temperature limit response time in seconds
 */
void Configuration::setTemperatureLimitSeconds(double _temperatureLimitSeconds) {
    this->temperatureLimitSeconds = _temperatureLimitSeconds;
    logger->debug("Set temperature limit response time to {} seconds", this->temperatureLimitSeconds);
}

void Configuration::resolveNetworkInterfaceDetails() {
    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) != 0) {
        const std::string errorMessage =
            fmt::format("getifaddrs() failed while resolving network interface '{}': {} (errno {})",
                        this->networkDeviceName, strerror(errno), errno);
        logger->critical(errorMessage);
        throw std::runtime_error(errorMessage);
    }

    bool foundIp = false;
    int interfacesScanned = 0;

    for (auto *p = ifap; p != nullptr; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET)
            continue;
        if (!p->ifa_name)
            continue;

        interfacesScanned++;

        if (this->networkDeviceName == p->ifa_name) {
            char buf[INET_ADDRSTRLEN];
            auto *sin = reinterpret_cast<sockaddr_in *>(p->ifa_addr);
            if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
                this->networkDeviceIPAddress = buf;
                foundIp = true;
                logger->debug("Resolved IP address for interface '{}': {}", this->networkDeviceName, buf);
                break;
            } else {
                const std::string errorMessage = fmt::format("inet_ntop() failed for interface '{}': {} (errno {})",
                                                             this->networkDeviceName, strerror(errno), errno);
                logger->critical(errorMessage);
                freeifaddrs(ifap);
                throw std::runtime_error(errorMessage);
            }
        }
    }

    if (!foundIp) {
        const std::string errorMessage =
            fmt::format("Could not find an IPv4 address for network interface '{}'. Interfaces scanned: {}",
                        this->networkDeviceName, interfacesScanned);
        logger->critical(errorMessage);
        freeifaddrs(ifap);
        throw std::runtime_error(errorMessage);
    }

    freeifaddrs(ifap);

    this->networkDeviceIndex = if_nametoindex(this->networkDeviceName.c_str());
    if (this->networkDeviceIndex == 0) {
        const std::string errorMessage = fmt::format("if_nametoindex() failed for interface '{}': {} (errno {})",
                                                     this->networkDeviceName, strerror(errno), errno);
        logger->critical(errorMessage);
        throw std::runtime_error(errorMessage);
    }

    logger->info("Resolved interface '{}' to IP {} and index {}", this->networkDeviceName, this->networkDeviceIPAddress,
                 this->networkDeviceIndex);
}

} // namespace creatures::config
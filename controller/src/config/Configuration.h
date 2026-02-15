#pragma once

#include <string>
#include <vector>

#include "config/UARTDevice.h"
#include "creature/Creature.h"

#include "audio/audio-config.h"
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
    bool getUseAudioSubsystem() const;
    u8 getSoundDeviceNumber() const;

    // Networking details
    std::string getNetworkDeviceIPAddress() const;
    uint getNetworkDeviceIndex() const;
    std::string getNetworkDeviceName();

    u16 getUniverse() const;
    [[nodiscard]] std::vector<UARTDevice> getUARTDevices() const;
    std::shared_ptr<creatures::creature::Creature> getCreature() const;
    std::string getCreatureConfigFile() const;
    bool isUsingServer() const;
    std::string getServerAddress() const;
    u16 getServerPort() const;

    // Watchdog configuration getters
    [[nodiscard]] bool getWatchdogDisabled() const;
    [[nodiscard]] double getPowerDrawLimitWatts() const;
    [[nodiscard]] double getPowerDrawWarningWatts() const;
    [[nodiscard]] double getPowerDrawResponseSeconds() const;
    [[nodiscard]] double getTemperatureLimitDegrees() const;
    [[nodiscard]] double getTemperatureWarningDegrees() const;
    [[nodiscard]] double getTemperatureLimitSeconds() const;

    void setUseGPIO(bool _useGPIO);
    void setUseAudioSubsystem(bool _useAudioSubsystem);
    void setSoundDeviceNumber(u8 _soundDeviceNumber);
    void setNetworkDeviceName(const std::string &_deviceName);
    void setUniverse(u16 _universe);
    void addUARTDevice(UARTDevice _uartDevice);
    void setCreature(std::shared_ptr<creatures::creature::Creature> _creature);
    void setCreatureConfigFile(std::string _creatureConfigFile);
    void setUseServer(bool _useServer);
    void setServerAddress(std::string _serverAddress);
    void setServerPort(u16 _serverPort);

    // Watchdog configuration setters
    void setWatchdogDisabled(bool _watchdogDisabled);
    void setPowerDrawLimitWatts(double _powerDrawLimitWatts);
    void setPowerDrawWarningWatts(double _powerDrawWarningWatts);
    void setPowerDrawResponseSeconds(double _powerDrawResponseSeconds);
    void setTemperatureLimitDegrees(double _temperatureLimitDegrees);
    void setTemperatureWarningDegrees(double _temperatureWarningDegrees);
    void setTemperatureLimitSeconds(double _temperatureLimitSeconds);

  private:
    /**
     * Look up the IP address and the interface index from the network interface supplied
     * in the configuration.
     */
    void resolveNetworkInterfaceDetails();

    // Should we use the GPIO pins?
    bool useGPIO = false;

    // Do we use the audio subsystem?
    bool useAudioSubsystem = false;

    u8 soundDeviceNumber = audio::DEFAULT_SOUND_DEVICE_NUMBER;

    // Which network interface to bind to?
    std::string networkDeviceName = DEFAULT_NETWORK_INTERFACE_NAME;
    std::string networkDeviceIPAddress = DEFAULT_NETWORK_DEVICE_IP_ADDRESS;
    uint networkDeviceIndex = 0;

    // Which e1.31 universe are we using?
    u16 universe = DEFAULT_UNIVERSE;

    // UARTs
    std::vector<UARTDevice> uartDevices;

    std::shared_ptr<Logger> logger;

    std::shared_ptr<creatures::creature::Creature> creature;

    // The creature's configuration file
    std::string creatureConfigFile;

    // The IP address of the server, if it's on
    bool useServer = false;
    std::string serverAddress;
    u16 serverPort;

    // Watchdog configuration
    bool watchdogDisabled = false;
    double powerDrawLimitWatts = 0.0;
    double powerDrawWarningWatts = 0.0;
    double powerDrawResponseSeconds = 0.0;
    double temperatureLimitDegrees = 0.0;
    double temperatureWarningDegrees = 0.0;
    double temperatureLimitSeconds = 0.0;
};

} // namespace creatures::config
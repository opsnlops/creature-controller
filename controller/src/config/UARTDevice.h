
#pragma once

#include <string>

#include "controller-config.h"
#include "logging/Logger.h"

namespace creatures ::config {

/**
 * UART Device Configuration
 *
 * This class represents one UART device, which is almost certainly
 * connected to a Pi Pico.
 */
class UARTDevice {
  public:
    explicit UARTDevice(std::shared_ptr<Logger> logger);

    ~UARTDevice();

    UARTDevice(const UARTDevice &other);

    friend class ConfigurationBuilder;

    enum module_name { A, B, C, D, E, F, invalid_module };

    [[nodiscard]] std::string getDeviceNode() const;
    [[nodiscard]] module_name getModule() const;
    [[nodiscard]] bool getEnabled() const;

    // Convert a string into a module name
    static module_name stringToModuleName(const std::string &typeStr);

    // A constexpr function to convert module_name to a char*
    static constexpr const char *moduleNameToString(module_name mod) {
        constexpr const char *names[] = {"A", "B", "C", "D", "E", "F", "invalid_module"};
        return (static_cast<std::size_t>(mod) < std::size(names)) ? names[mod] : "unknown";
    }

  protected:
    void setDeviceNode(std::string _deviceNode);
    void setModule(module_name _module);
    void setEnabled(bool _enabled);

  private:
    bool enabled;
    std::string deviceNode;
    module_name module;

    std::shared_ptr<Logger> logger;
};

} // namespace creatures::config

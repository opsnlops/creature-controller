
#pragma once

#include <string>

#include "logging/Logger.h"
#include "controller-config.h"

namespace creatures :: config {

    /**
     * UART Device Configuration
     *
     * This class represents one UART device, which is almost certainly
     * connected to a Pi Pico.
     */
    class UARTDevice {
    public:

        UARTDevice(std::shared_ptr <Logger> logger);

        ~UARTDevice();

        UARTDevice(const UARTDevice &other);

        friend class ConfigurationBuilder;

        enum module_name {
            A,
            B,
            C,
            D,
            E,
            F,
            invalid_module
        };

        std::string getDeviceNode() const;
        module_name getModule() const;
        bool getEnabled() const;

        // Convert a string into a module name
        static module_name stringToModuleName(const std::string &typeStr);

    protected:
        void setDeviceNode(std::string _deviceNode);
        void setModule(module_name _module);
        void setEnabled(bool _enabled);

    private:
        bool enabled;
        std::string deviceNode;
        module_name module;

        std::shared_ptr <Logger> logger;
    };

} // creatures :: config

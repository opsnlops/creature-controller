
#pragma once

#include <string>

#include "controller-config.h"
#include "logging/Logger.h"

namespace creatures {

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

        friend class CommandLine;

        enum module_name {
            A,
            B,
            C,
            D,
            E,
            F
        };

        std::string getDeviceNode() const;
        module_name getModule() const;
        bool getEnabled() const;

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
}
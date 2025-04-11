#pragma once

#include <string>
#include <thread>

#include "controller-config.h"
#include "logging/Logger.h"
#include "config/UARTDevice.h"
#include "util/StoppableThread.h"

// Forward declaration with proper namespace
namespace creatures {
    class SerialHandler;
}

namespace creatures::io {

    using creatures::config::UARTDevice;

    /**
     * A thread that monitors the serial port connection and
     * triggers reconnection when necessary
     */
    class SerialPortMonitor : public StoppableThread {

    public:
        SerialPortMonitor(const std::shared_ptr<Logger>& logger,
                          std::string deviceNode,
                          UARTDevice::module_name moduleName,
                          creatures::SerialHandler& serialHandler);

        ~SerialPortMonitor() override {
            this->logger->info("SerialPortMonitor destroyed");
        }

        void start() override;

    protected:
        void run() override;

    private:
        std::shared_ptr<Logger> logger;
        std::string deviceNode;
        UARTDevice::module_name moduleName;
        creatures::SerialHandler& serialHandler; // Reference to the serial handler

        // Monitor constants are defined in controller-config.h
    };

} // creatures::io
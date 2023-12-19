#pragma once

#include <string>

#include "logging/Logger.h"

namespace creatures {

    class Configuration {

    public:

        Configuration(std::shared_ptr<Logger> logger);

        friend class CommandLine;

        std::string getConfigFileName() const;
        std::string getUsbDevice() const;

    protected:
        void setConfigFileName(std::string _configFileName);
        void setUsbDevice(std::string _usbDevice);

    private:

        // The device node of our USB device
        std::string usbDevice;

        // The location of our JSON config file
        std::string configFileName;

        std::shared_ptr<Logger> logger;

    };


}
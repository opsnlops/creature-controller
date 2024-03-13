#pragma once

#include <argparse/argparse.hpp>

#include "Configuration.h"
#include "logging/Logger.h"

#include "creature/Creature.h"

namespace creatures {

    class CommandLine {

        class Configuration; // Forward declaration

    public:
        explicit CommandLine(std::shared_ptr<Logger> logger);
        std::shared_ptr<config::Configuration> parseCommandLine(int argc, char **argv);

        void listNetworkDevices();
        std::string getVersion();

    private:
        std::shared_ptr<Logger> logger;
    };



}
#pragma once

#include <argparse/argparse.hpp>

#include "Configuration.h"
#include "logging/Logger.h"
#include "util/Result.h"

#include "creature/Creature.h"

namespace creatures {

    class CommandLine {

        class Configuration; // Forward declaration

    public:
        explicit CommandLine(std::shared_ptr<Logger> logger);
        Result<std::shared_ptr<config::Configuration>> parseCommandLine(int argc, char **argv);

        void listNetworkDevices();
        static std::string getVersion();

    private:
        std::shared_ptr<Logger> logger;
    };



}
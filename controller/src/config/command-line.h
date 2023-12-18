#pragma once

#include <argparse/argparse.hpp>

#include "config.h"
#include "logging/Logger.h"

#include "creature/creature.h"

namespace creatures {

    class CommandLine {

    public:
        explicit CommandLine(std::shared_ptr<Logger> logger);
        std::shared_ptr<Configuration> parseCommandLine(int argc, char **argv);


    private:
        std::shared_ptr<Logger> logger;
        std::shared_ptr<Creature> parseConfigFile(std::string configFilename);

    };



}
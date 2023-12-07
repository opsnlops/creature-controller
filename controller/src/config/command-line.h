#pragma once

#include <argparse/argparse.hpp>

#include "config.h"
#include "namespace-stuffs.h"

#include "creature/creature.h"

namespace creatures {

    class CommandLine {

    public:
        static std::shared_ptr<Configuration> parseCommandLine(int argc, char **argv);


    private:

        static std::shared_ptr<Creature> parseConfigFile(std::string configFilename);

    };



}
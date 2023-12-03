
#include <iostream>
#include <string>

#include <argparse/argparse.hpp>
#include <utility>

#include "config.h"
#include "command-line.h"

/*
 * This is using argparse:
 *   https://github.com/p-ranav/argparse
 */


#include "creature/parrot.h"
#include "creature_builder.h"
#include "CreatureBuilderException.h"

namespace creatures {

    std::shared_ptr<Configuration> CommandLine::parseCommandLine(int argc, char **argv) {

        auto config = std::make_shared<Configuration>();

        argparse::ArgumentParser program("creature-controller");

        // Only allow one i2c bus arg
        auto &i2c_group = program.add_mutually_exclusive_group(true);

        i2c_group.add_argument("-m", "--mock")
                .help("use the mock i2c bus")
                .default_value(false)
                .implicit_value(true);

#ifdef __linux__
        i2c_group.add_argument("-s", "--smbus")
                .help("use the Linux SMBus for i2c on the given device node")
                .nargs(1)
                .metavar("DEVICE_NODE");

        i2c_group.add_argument("-b", "--bcm2835")
                .help("use the bcm2835 driver on a Pi")
                .implicit_value(true)
                .default_value(false);
#endif

        program.add_argument("-c", "--creature-config")
                .help("JSON file for this creature")
                .required();

        program.add_description("This application is the Linux version of the Creature Controller that's part\n"
                                "of April's Creature Workshop! 🐰");
        program.add_epilog("🦜 Bawk!");

        try {
            program.parse_args(argc, argv);
        }
        catch (const std::exception &err) {

            critical(err.what());

            std::cerr << "\n" << program;
            std::exit(1);
        }


        // Parse out the i2c bus type. Only one of these can/will be valid because of the
        // mutually exclusive group above.
        if(program.get<bool>("-m")) {
            config->setI2CBusType(Configuration::I2CBusType::mock);
            info("using the mock i2c bus");
        }

#ifdef __linux__
        if(program.is_used("-s")) {
            auto smbus = program.get<std::string>("-s");
            if (smbus.length() > 0) {
                config->setI2CBusType(Configuration::I2CBusType::smbus);
                config->setSMBusDeviceNode(smbus);
                info("using the Linux smbus i2c on {}", smbus);
            }
        }

        if(program.get<bool>("-b")) {
            config->setI2CBusType(Configuration::I2CBusType::bcm2835);
            info("using the bcm2835 i2c bus");
        }
#endif

        // Parse out the creature config file
        auto creatureFile = program.get<std::string>("-c");
        debug("read creature file {} from command line", creatureFile);
        if(creatureFile.length() > 0) {
            config->setConfigFileName(creatureFile);
            info("set our creature config file to {}", creatureFile);
        }


        return config;
    }

};
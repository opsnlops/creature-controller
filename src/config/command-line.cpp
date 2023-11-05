
#include <iostream>
#include <string>

#include <argparse/argparse.hpp>

#include "config.h"
#include "command-line.h"

namespace creatures {

    std::shared_ptr<Configuration> CommandLine::parseCommandLine(int argc, char **argv) {

        auto config = std::make_shared<Configuration>();

        argparse::ArgumentParser program(argv[0]);

        auto &i2c_group = program.add_mutually_exclusive_group(true);
        i2c_group.add_argument("--mock");
        i2c_group.add_argument("--smbus");
        i2c_group.add_argument("--bcm2835");

        program.add_argument("-i", "--i2c")
                .default_value(std::string("mock"))
                .required()
                .help("which i2c bus to use (mock, smbus, bcm2835)");

        program.add_epilog("🦜 Bawk!");

        std::cout << program;

        return config;
    }

};
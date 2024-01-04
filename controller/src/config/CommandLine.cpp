
#include <iostream>
#include <string>

#include <argparse/argparse.hpp>
#include <utility>

#include "Configuration.h"
#include "CommandLine.h"
#include "logging/Logger.h"

/*
 * This is using argparse:
 *   https://github.com/p-ranav/argparse
 */


namespace creatures {

    CommandLine::CommandLine(std::shared_ptr<Logger> logger) : logger(logger) {}


    std::shared_ptr<Configuration> CommandLine::parseCommandLine(int argc, char **argv) {

        auto config = std::make_shared<Configuration>(logger);

        argparse::ArgumentParser program("creature-controller");


        program.add_argument("-c", "--creature-config")
                .help("JSON file for this creature")
                .required();

        program.add_argument("-u", "--usb-device")
                .help("USB device for this creature")
                .default_value("/dev/tty.usbmodem101")
                .required();

        program.add_argument("-g", "--use-gpio")
                .help("Use the GPIO pins? (RPI only!)")
                .default_value(false)
                .implicit_value(true);

        program.add_description("This application is the Linux version of the Creature Controller that's part\n"
                                "of April's Creature Workshop! 🐰");
        program.add_epilog("🦜 Bawk!");

        try {
            program.parse_args(argc, argv);
        }
        catch (const std::exception &err) {

            logger->critical(err.what());

            std::cerr << "\n" << program;
            std::exit(1);
        }



        // Parse out the creature config file
        auto creatureFile = program.get<std::string>("-c");
        logger->debug("read creature file {} from command line", creatureFile);
        if(!creatureFile.empty()) {
            config->setConfigFileName(creatureFile);
            logger->info("set our creature config file to {}", creatureFile);
        }

        auto usbDevice = program.get<std::string>("-u");
        logger->debug("read usb device {} from command line", usbDevice);
        if(!usbDevice.empty()) {
            config->setUsbDevice(usbDevice);
            logger->info("set our usb device to {}", usbDevice);
        }

        auto useGPIO = program.get<bool>("-g");
        logger->debug("read use GPIO {} from command line", useGPIO);
        if(useGPIO) {
            config->setUseGPIO(useGPIO);
            logger->info("set our use GPIO to {}", useGPIO);
        }

        return config;
    }
}
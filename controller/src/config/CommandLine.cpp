
#include <cstdio>
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <string>
#include <utility>

#include <argparse/argparse.hpp>

#include "Configuration.h"
#include "CommandLine.h"
#include "Version.h"
#include "logging/Logger.h"

/*
 * This is using argparse:
 *   https://github.com/p-ranav/argparse
 */


namespace creatures {

    CommandLine::CommandLine(std::shared_ptr<Logger> logger) : logger(logger) {}


    std::shared_ptr<Configuration> CommandLine::parseCommandLine(int argc, char **argv) {

        auto config = std::make_shared<Configuration>(logger);

        argparse::ArgumentParser program("creature-controller", getVersion());

        program.add_argument("-c", "--creature-config")
                .help("JSON file for this creature")
                .required();

        program.add_argument("-u", "--usb-device")
                .help("USB device for this creature")
                .default_value("/dev/tty.usbmodem101")
                .nargs(1)
                .required();

        program.add_argument("-n", "--network-device")
                .help("network device to use")
                .default_value(DEFAULT_NETWORK_DEVICE_NUMBER)
                .nargs(1)
                .scan<'i', int>();

        program.add_argument("-g", "--use-gpio")
                .help("Use the GPIO pins? (RPI only!)")
                .default_value(false)
                .implicit_value(true);

        program.add_argument("--list-network-devices")
                .help("list available network devices and exit")
                .default_value(false)
                .implicit_value(true);

        program.add_description("This application is the Linux version of the Creature Controller that's part\n"
                                "of April's Creature Workshop! 🐰");
        program.add_epilog("This is version " + getVersion() + "\n\n" +
                           "🦜 Bawk!");

        try {
            program.parse_args(argc, argv);
        }
        catch (const std::exception &err) {

            logger->critical(err.what());
            std::cerr << "\n" << program;
            std::exit(1);
        }


        if(program.get<bool>("--list-network-devices")) {
            listNetworkDevices();
            std::exit(0);
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

        auto networkDevice = program.get<int>("-n");
        logger->debug("read network device {} from command line", networkDevice);
        if(networkDevice > 0) {
            config->setNetworkDevice(networkDevice);
            logger->debug("set our network device to {}", networkDevice);
        }


        return config;
    }

    void CommandLine::listNetworkDevices() {

        struct ifaddrs *ifaddr, *ifa;

        if (getifaddrs(&ifaddr) == -1) {
            logger->critical("Unable to get network devices: {}", strerror(errno));
        }

        std::cout << "List of network devices:" << std::endl;

        int n;

        // Walk the list
        for (ifa = ifaddr, n = 0; ifa != nullptr; ifa = ifa->ifa_next, n++) {
            if (ifa->ifa_addr == nullptr)
                continue;

            // Could be used to limit it to IPv4 or IPv6
            //int family = ifa->ifa_addr->sa_family;

            // Print out the name and index
            std::cout << " Device: " << if_nametoindex(ifa->ifa_name) << ", Name: " << ifa->ifa_name << std::endl;
        }

        freeifaddrs(ifaddr);

    }

    std::string CommandLine::getVersion() {
        return fmt::format("{}.{}.{}",
                           CREATURE_CONTROLLER_VERSION_MAJOR,
                           CREATURE_CONTROLLER_VERSION_MINOR,
                           CREATURE_CONTROLLER_VERSION_PATCH);
    }
}
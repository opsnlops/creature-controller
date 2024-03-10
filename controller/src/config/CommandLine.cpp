
#include <algorithm>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <string>
#include <utility>
#include <vector>

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

        program.add_argument("-i", "--network-ip-address")
                .help("host IP address to bind to")
                .default_value(DEFAULT_NETWORK_DEVICE_IP_ADDRESS)
                .nargs(1);

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

        auto networkDeviceIPAddress = program.get<std::string>("-i");
        logger->debug("read network IP {} from command line", networkDeviceIPAddress);
        if(!networkDeviceIPAddress.empty()) {
            config->setNetworkDeviceIPAddress(networkDeviceIPAddress);
            logger->debug("set our network IP to {}", networkDeviceIPAddress);
        }


        return config;
    }

    void CommandLine::listNetworkDevices() {
        struct ifaddrs *ifaddr, *ifa;
        char addrBuff[INET6_ADDRSTRLEN];

        if (getifaddrs(&ifaddr) == -1) {
            logger->critical("Unable to get network devices: {}", strerror(errno));
            return;
        }

        // Map to store device name, index, and IP addresses
        std::map<std::string, std::pair<int, std::vector<std::string>>> interfaces;

        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr)
                continue;

            void *tmpAddrPtr = nullptr;
            bool isIPv4 = false;

            // Check if it is IP4 or IP6 and set tmpAddrPtr accordingly
            if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
                tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                isIPv4 = true;
            } else if (ifa->ifa_addr->sa_family == AF_INET6) { // IPv6
                tmpAddrPtr = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            }

            if (tmpAddrPtr) {
                inet_ntop(ifa->ifa_addr->sa_family, tmpAddrPtr, addrBuff, sizeof(addrBuff));
                // Prioritize IPv4 by inserting at the beginning of the vector
                if (isIPv4) {
                    interfaces[ifa->ifa_name].second.insert(interfaces[ifa->ifa_name].second.begin(), addrBuff);
                } else {
                    interfaces[ifa->ifa_name].second.push_back(addrBuff);
                }
                interfaces[ifa->ifa_name].first = if_nametoindex(ifa->ifa_name);
            }
        }

        freeifaddrs(ifaddr);

        std::cout << "List of network devices:" << std::endl;
        for (const auto &iface : interfaces) {
            std::cout << " Name: " << iface.first;
            std::cout << ", IPs: ";
            for (const auto &ip : iface.second.second) {
                std::cout << ip << " ";
            }
            std::cout << std::endl;
        }
    }

    std::string CommandLine::getVersion() {
        return fmt::format("{}.{}.{}",
                           CREATURE_CONTROLLER_VERSION_MAJOR,
                           CREATURE_CONTROLLER_VERSION_MINOR,
                           CREATURE_CONTROLLER_VERSION_PATCH);
    }
}
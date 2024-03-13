
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


#include "ConfigurationBuilder.h"
#include "CommandLine.h"
#include "Version.h"
#include "logging/Logger.h"

/*
 * This is using argparse:
 *   https://github.com/p-ranav/argparse
 */


namespace creatures {

    class Configuration; // Forward declaration

    CommandLine::CommandLine(std::shared_ptr<Logger> logger) : logger(logger) {}


    std::shared_ptr<config::Configuration> CommandLine::parseCommandLine(int argc, char **argv) {

        argparse::ArgumentParser program("creature-controller", getVersion());

        program.add_argument("--creature-config")
                .help("JSON file for this creature")
                .nargs(1)
                .required();

        program.add_argument("--config")
                .help("Our configuration file")
                .nargs(1)
                .required();

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


        // Parse out the  config file
        auto configFile = program.get<std::string>("--config");
        logger->debug("config file {} from command line", configFile);
        if(!configFile.empty()) {
            logger->info("set our config file to {}", configFile);
        }

        // Parse out the creature config file
        auto creatureConfigFile = program.get<std::string>("--creature-config");
        logger->debug("read creature file {} from command line", creatureConfigFile);
        if(!creatureConfigFile.empty()) {
            logger->info("set our creature config file to {}", creatureConfigFile);
        }


        // Make the builder
        auto configBuilder = std::make_unique<creatures::config::ConfigurationBuilder>(logger, configFile);
        auto config = configBuilder->build();

        config->setCreatureConfigFile(creatureConfigFile);

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

// Standard library includes
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Network-related includes
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

// Third-party includes
#include <SDL.h>
#include <argparse/argparse.hpp>

// Project includes
#include "CommandLine.h"
#include "ConfigurationBuilder.h"
#include "Version.h"
#include "audio/audio-config.h"
#include "logging/Logger.h"
#include "logging/SpdlogLogger.h"
#include "util/Result.h"

/*
 * This is using argparse:
 *   https://github.com/p-ranav/argparse
 */

namespace creatures {

class Configuration; // Forward declaration

CommandLine::CommandLine(std::shared_ptr<Logger> logger) : logger(std::move(logger)) {
    if (!this->logger) {
        this->logger = std::make_shared<creatures::SpdlogLogger>();
    }
}

Result<std::shared_ptr<config::Configuration>> CommandLine::parseCommandLine(int argc, char **argv) {
    argparse::ArgumentParser program("creature-controller", getVersion());

    setupCommandLineArguments(program);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &err) {
        logger->critical("Command line parsing error: {}", err.what());
        std::cerr << "\n" << program;
        return Result<std::shared_ptr<config::Configuration>>(
            ControllerError(ControllerError::InvalidConfiguration, err.what()));
    }

    if (program.get<bool>("--list-network-devices")) {
        listNetworkDevices();
        std::exit(0);
    }

    if (program.get<bool>("--list-sound-devices")) {
        listAudioDevices();
        std::exit(0);
    }

    // Parse out the config files
    auto configFile = program.get<std::string>("--config");
    auto creatureConfigFile = program.get<std::string>("--creature-config");

    logger->debug("Config file: {}, Creature config file: {}", configFile, creatureConfigFile);

    if (!configFile.empty()) {
        logger->info("Using config file: {}", configFile);
    }

    if (!creatureConfigFile.empty()) {
        logger->info("Using creature config file: {}", creatureConfigFile);
    }

    // Build the configuration
    auto configBuilder = std::make_unique<creatures::config::ConfigurationBuilder>(logger, configFile);
    auto configResult = configBuilder->build();

    if (!configResult.isSuccess()) {
        return configResult;
    }

    // Set the creature config file in the configuration object
    configResult.getValue().value()->setCreatureConfigFile(creatureConfigFile);

    return configResult;
}

void CommandLine::setupCommandLineArguments(argparse::ArgumentParser &program) {
    program.add_argument("--creature-config").help("JSON file for this creature").nargs(1).required();

    program.add_argument("--config").help("Our configuration file").nargs(1).required();

    program.add_argument("--list-network-devices")
        .help("List available network devices and exit")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--list-sound-devices")
        .help("List available sound devices and exit")
        .default_value(false)
        .implicit_value(true);

    program.add_description("This application is the Linux version of the Creature Controller that's part\n"
                            "of April's Creature Workshop! 🐰");
    program.add_epilog("This is version " + getVersion() + "\n\n" + "🦜 Bawk!");
}

void CommandLine::listNetworkDevices() {
    struct ifaddrs *ifaddr = nullptr;
    char addrBuff[INET6_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        logger->critical("Unable to get network devices: {}", strerror(errno));
        return;
    }

    // Use RAII to ensure ifaddr is freed
    struct IfaddrGuard {
        struct ifaddrs *addr;
        explicit IfaddrGuard(struct ifaddrs *addr) : addr(addr) {}
        ~IfaddrGuard() {
            if (addr)
                freeifaddrs(addr);
        }
    } guard(ifaddr);

    // Map to store device name, index, and IP addresses
    std::map<std::string, std::pair<int, std::vector<std::string>>> interfaces;

    // Collect network interface information
    collectNetworkInterfaces(ifaddr, interfaces, addrBuff);

    // Display the collected information
    displayNetworkInterfaces(interfaces);
}

void CommandLine::listAudioDevices() {
    std::cout << "Available audio devices for RTP playback:" << std::endl;

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return;
    }

    int numDevices = SDL_GetNumAudioDevices(0);

    std::cout << "Number of audio devices: " << numDevices << std::endl;
    for (int i = 0; i < numDevices; ++i) {
        const char *deviceName = SDL_GetAudioDeviceName(i, 0);
        if (deviceName) {
            std::cout << "  Device " << i << ": " << deviceName << std::endl;
        }
    }

    SDL_Quit();
}

void CommandLine::collectNetworkInterfaces(struct ifaddrs *ifaddr,
                                           std::map<std::string, std::pair<int, std::vector<std::string>>> &interfaces,
                                           char *addrBuff) {

    for (auto ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
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
            inet_ntop(ifa->ifa_addr->sa_family, tmpAddrPtr, addrBuff, INET6_ADDRSTRLEN);
            // Prioritize IPv4 by inserting at the beginning of the vector
            if (isIPv4) {
                interfaces[ifa->ifa_name].second.insert(interfaces[ifa->ifa_name].second.begin(), addrBuff);
            } else {
                interfaces[ifa->ifa_name].second.push_back(addrBuff);
            }
            interfaces[ifa->ifa_name].first = if_nametoindex(ifa->ifa_name);
        }
    }
}

void CommandLine::displayNetworkInterfaces(
    const std::map<std::string, std::pair<int, std::vector<std::string>>> &interfaces) {

    std::cout << "List of network devices:" << std::endl;
    for (const auto &iface : interfaces) {
        std::cout << " Name: " << iface.first;
        std::cout << ", IPs: ";

        if (iface.second.second.empty()) {
            std::cout << "none";
        } else {
            for (size_t i = 0; i < iface.second.second.size(); ++i) {
                std::cout << iface.second.second[i];
                if (i < iface.second.second.size() - 1) {
                    std::cout << ", ";
                }
            }
        }
        std::cout << std::endl;
    }
}

std::string CommandLine::getVersion() {
    return fmt::format("{}.{}.{}", CREATURE_CONTROLLER_VERSION_MAJOR, CREATURE_CONTROLLER_VERSION_MINOR,
                       CREATURE_CONTROLLER_VERSION_PATCH);
}
} // namespace creatures
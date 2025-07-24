#pragma once

// Standard library
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Network-related includes
#include <ifaddrs.h>
#include <netinet/in.h>

// Third-party includes
#include <argparse/argparse.hpp>

// Project includes
#include "Configuration.h"
#include "logging/Logger.h"
#include "util/Result.h"

namespace creatures {

namespace config {
class Configuration; // Proper forward declaration with namespace
}

/**
 * @class CommandLine
 * @brief Handles command-line argument parsing and network device discovery
 */
class CommandLine {
  public:
    /**
     * @brief Constructor
     * @param logger Logger instance for the class
     */
    explicit CommandLine(std::shared_ptr<Logger> logger);

    /**
     * @brief Parse command line arguments and build configuration
     * @param argc Argument count
     * @param argv Argument values
     * @return Result containing Configuration or error
     */
    Result<std::shared_ptr<config::Configuration>> parseCommandLine(int argc, char **argv);

    /**
     * @brief Lists all network devices and their IP addresses
     */
    void listNetworkDevices();

    static void listAudioDevices();

    /**
     * @brief Get the version string
     * @return Version in format "MAJOR.MINOR.PATCH"
     */
    [[nodiscard]] static std::string getVersion();

  private:
    std::shared_ptr<Logger> logger;

    /**
     * @brief Set up command line arguments for argparse
     * @param program The argument parser instance
     */
    static void setupCommandLineArguments(argparse::ArgumentParser &program);

    /**
     * @brief Collect network interface information
     * @param ifaddr Pointer to ifaddrs structures
     * @param interfaces Map to store interface information
     * @param addrBuff Buffer for address string conversion
     */
    void collectNetworkInterfaces(struct ifaddrs *ifaddr,
                                  std::map<std::string, std::pair<int, std::vector<std::string>>> &interfaces,
                                  char *addrBuff);

    /**
     * @brief Display collected network interface information
     * @param interfaces Map containing interface information
     */
    void displayNetworkInterfaces(const std::map<std::string, std::pair<int, std::vector<std::string>>> &interfaces);
};

} // namespace creatures
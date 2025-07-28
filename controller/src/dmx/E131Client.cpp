//
// E131Client.cpp
//

#include <arpa/inet.h>
#include <cerrno>       // For errno
#include <cstring>      // For strerror
#include <netinet/in.h> // For network structures
#include <sys/socket.h> // For socket error constants
#include <thread>
#include <unordered_map>
#include <utility>

#include "e131.h"

#include "creature/Creature.h"
#include "dmx/E131Client.h"
#include "dmx/E131Exception.h"
#include "logging/Logger.h"
#include "util/thread_name.h"

#include "controller-config.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "LoopDoesntUseConditionVariableInspection"

namespace creatures::dmx {

E131Client::E131Client(const std::shared_ptr<Logger> &logger) : logger(logger) {
    this->logger->info("e1.31 client created");
}

E131Client::~E131Client() { this->logger->info("e1.31 client destroyed"); }

void E131Client::init(const std::shared_ptr<creature::Creature> &_creature,
                      const std::shared_ptr<Controller> &_controller, std::string _networkInterfaceName,
                      uint _networkInterfaceIndex, std::string _networkInterfaceAddress) {
    this->creature = _creature;
    this->controller = _controller;
    this->networkInterfaceName = _networkInterfaceName;
    this->networkInterfaceIndex = _networkInterfaceIndex;
    this->networkInterfaceAddress = _networkInterfaceAddress;

    // Create our input map
    for (const auto &input : this->creature->getInputs()) {
        // Make a new Input and copy the data into it
        auto newInput = Input(input);
        this->inputMap.emplace(newInput.getSlot(), newInput);
    }
    logger->debug("e1.31 client init'ed with {} inputs", this->inputMap.size());
}

void E131Client::start() {
    // Make sure we have our creature and controller
    if (this->creature == nullptr) {
        throw E131Exception("Unable to start e1.31 client without a creature");
    }
    if (this->controller == nullptr) {
        throw E131Exception("Unable to start e1.31 client without a controller");
    }

    this->logger->info("e1.31 client started with {} inputs", this->inputMap.size());

    // Start the worker
    StoppableThread::start();
}

// Helper function to get detailed error information
std::string getDetailedSocketError(const std::string &operation) {
    int errorCode = errno;
    std::string baseError = strerror(errorCode);

    // Add specific socket error descriptions
    std::string detailedError = fmt::format("{} failed: {} (errno: {})", operation, baseError, errorCode);

    // Add common socket error explanations
    switch (errorCode) {
    case ENODEV:
        detailedError += " - Network device not found or not available";
        break;
    case EADDRNOTAVAIL:
        detailedError += " - Address not available (check IP address)";
        break;
    case ENETDOWN:
        detailedError += " - Network interface is down";
        break;
    case ENETUNREACH:
        detailedError += " - Network is unreachable";
        break;
    case EACCES:
        detailedError += " - Permission denied (may need root/sudo)";
        break;
    case EINVAL:
        detailedError += " - Invalid argument (check multicast address/interface)";
        break;
    case ENOPROTOOPT:
        detailedError += " - Protocol option not supported";
        break;
    default:
        break;
    }

    return detailedError;
}

// Helper function to validate network configuration
void validateNetworkConfig(const std::shared_ptr<Logger> &logger, uint16_t universe, const std::string &interfaceIP) {
    logger->info("Network configuration validation:");
    logger->info("  Universe: {}", universe);
    logger->info("  Interface IP: {}", interfaceIP);
    logger->info("  Multicast group: 239.255.{}.{}", (universe >> 8) & 0xFF, universe & 0xFF);

    // Validate IP address format (basic check)
    if (interfaceIP.empty()) {
        logger->warn("  Interface IP is empty");
    }

    // Check universe range
    if (universe == 0 || universe > 63999) {
        logger->warn("  Universe {} is outside standard range (1-63999)", universe);
    }
}

void E131Client::run() {
    setThreadName("E131Client::run");

    logger->info("e1.31 worker thread starting");

    int sockfd = e131_socket();
    if (sockfd < 0) {
        const std::string errorMessage =
            fmt::format("Unable to create e1.31 socket: {} (errno {})", getDetailedSocketError("e131_socket"), errno);
        logger->critical(errorMessage);
        throw E131Exception(errorMessage);
    }

    logger->debug("E1.31 socket created (fd: {})", sockfd);

    if (e131_bind(sockfd, E131_DEFAULT_PORT) < 0) {
        const std::string errorMessage = fmt::format("Unable to bind E1.31 socket to port {}: {} (errno {})",
                                                     E131_DEFAULT_PORT, getDetailedSocketError("e131_bind"), errno);
        logger->critical(errorMessage);
        close(sockfd);
        throw E131Exception(errorMessage);
    }

    logger->debug("E1.31 socket bound to port {}", E131_DEFAULT_PORT);

    // Gather resolved interface info
    uint16_t universe = creature->getUniverse();

    logger->info("Joining multicast group for universe {} on interface '{}'", universe, networkInterfaceName);
    logger->info("  IP address: {}", networkInterfaceAddress);
    logger->info("  Interface index: {}", networkInterfaceIndex);
    logger->info("  Socket FD: {}", sockfd);

    // if (e131_multicast_join_iface(sockfd, universe, networkInterfaceIndex) < 0) {
    //     const std::string errorMessage =
    //         fmt::format("Failed to join multicast group for universe {} on interface '{}': {} (errno {})", universe,
    //                     networkInterfaceName, getDetailedSocketError("e131_multicast_join_iface"), errno);
    //     logger->critical(errorMessage);
    //     close(sockfd);
    //     throw E131Exception(errorMessage);
    // }


    // The above code isn't binding to the specific multicast group correctly, so let's do it manually.
    /// Start of manual multicast join
    struct ip_mreqn mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = htonl(0xEFFF0000 | universe); // 239.255.x.x format
    mreq.imr_address.s_addr = inet_addr(networkInterfaceAddress.c_str());
    mreq.imr_ifindex = networkInterfaceIndex;

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        const std::string errorMessage =
            fmt::format("Failed to join multicast group for universe {} on interface '{}': {} (errno {})",
                       universe, networkInterfaceName, getDetailedSocketError("setsockopt IP_ADD_MEMBERSHIP"), errno);
        logger->critical(errorMessage);
        close(sockfd);
        throw E131Exception(errorMessage);
    }
    /// End of manual multicast join


    logger->info("Successfully joined multicast group 239.255.0.{} on {}", universe, networkInterfaceAddress);
    logger->info("Waiting for E1.31 packets on interface '{}'", networkInterfaceName);

    // Receive loop
    e131_packet_t packet;
    e131_error_t error;
    uint8_t last_seq = 0;

    while (!stop_requested.load()) {
        if (e131_recv(sockfd, &packet) < 0) {
            logger->error("e131_recv() failed: {} (errno {})", getDetailedSocketError("e131_recv"), errno);
            continue;
        }

        if ((error = e131_pkt_validate(&packet)) != E131_ERR_NONE) {
            logger->warn("Invalid E1.31 packet: {}", e131_strerror(error));
            continue;
        }

        if (e131_pkt_discard(&packet, last_seq)) {
            logger->warn("Out-of-order packet received (seq: {}, last: {})", packet.frame.seq_number, last_seq);
            last_seq = packet.frame.seq_number;
            continue;
        }

        handlePacket(packet);
        last_seq = packet.frame.seq_number;
    }

    logger->info("e1.31 client shutting down");
    close(sockfd);
}

void E131Client::handlePacket(const e131_packet_t &packet) {
    std::string hexString;

    // TODO: Don't do this unless verbose is on
    for (u16 i = creature->getChannelOffset(); i < creature->getNumberOfServos() + creature->getChannelOffset(); i++) {
        hexString += fmt::format("{:#04x} ", packet.dmp.prop_val[i]);
    }
    logger->trace("Received e1.31 packet: {}", hexString);

    std::vector<Input> inputs;

    // Walk the input map
    for (auto &[fst, snd] : this->inputMap) {
        const u16 slot = fst + creature->getChannelOffset();
        const u8 value = packet.dmp.prop_val[slot];

        auto inputToSend = Input(snd);
        inputToSend.setIncomingRequest(value);

        inputs.push_back(inputToSend);
        // logger->trace("Setting input slot {} ({}) to value {}", slot, inputToSend.getName(), value);
    }

    this->controller->acceptInput(inputs);
}

} // namespace creatures::dmx

#pragma clang diagnostic pop
//
// E131Client.cpp
//

#include <thread>
#include <unordered_map>
#include <utility>
#include <cerrno>        // For errno
#include <cstring>       // For strerror
#include <sys/socket.h>  // For socket error constants
#include <netinet/in.h>  // For network structures

#include "e131.h"

#include "creature/Creature.h"
#include "dmx/E131Exception.h"
#include "dmx/E131Client.h"
#include "logging/Logger.h"
#include "util/thread_name.h"

#include "controller-config.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "LoopDoesntUseConditionVariableInspection"

namespace creatures::dmx {

    E131Client::E131Client(const std::shared_ptr<Logger>& logger) : logger(logger) {
        this->logger->info("e1.31 client created");
    }

    E131Client::~E131Client() {
        this->logger->info("e1.31 client destroyed");
    }

    void E131Client::init(const std::shared_ptr<creature::Creature>& _creature,
                          const std::shared_ptr<Controller>& _controller,
                          std::string _networkDeviceIPAddress) {
        this->creature = _creature;
        this->controller = _controller;
        this->networkDeviceIPAddress = std::move(_networkDeviceIPAddress);

        // Create our input map
        for( const auto& input : this->creature->getInputs() ) {
            // Make a new Input and copy the data into it
            auto newInput = Input(input);
            this->inputMap.emplace(newInput.getSlot(), newInput);
        }
        logger->debug("e1.31 client init'ed with {} inputs", this->inputMap.size());
    }

    void E131Client::start() {
        // Make sure we have our creature and controller
        if(this->creature == nullptr) {
            throw E131Exception("Unable to start e1.31 client without a creature");
        }
        if(this->controller == nullptr) {
            throw E131Exception("Unable to start e1.31 client without a controller");
        }

        this->logger->info("e1.31 client started with {} inputs", this->inputMap.size());

        // Start the worker
        StoppableThread::start();
    }

    // Helper function to get detailed error information
    std::string getDetailedSocketError(const std::string& operation) {
        int errorCode = errno;
        std::string baseError = strerror(errorCode);

        // Add specific socket error descriptions
        std::string detailedError = fmt::format("{} failed: {} (errno: {})",
                                               operation, baseError, errorCode);

        // Add common socket error explanations
        switch(errorCode) {
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
    void validateNetworkConfig(const std::shared_ptr<Logger>& logger,
                              uint16_t universe,
                              const std::string& interfaceIP) {
        logger->info("Network configuration validation:");
        logger->info("  Universe: {}", universe);
        logger->info("  Interface IP: {}", interfaceIP);
        logger->info("  Multicast group: 239.255.{}.{}",
                    (universe >> 8) & 0xFF, universe & 0xFF);

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

        this->logger->info("e1.31 worker thread going");

        int sockfd;
        e131_packet_t packet;
        e131_error_t error;
        u8 last_seq = 0x00;

        // Validate configuration before attempting connection
        validateNetworkConfig(logger, creature->getUniverse(), networkDeviceIPAddress);

        // Create an e1.31 socket with detailed error reporting
        logger->debug("Creating E1.31 socket...");
        if ((sockfd = e131_socket()) < 0) {
            const std::string errorMessage = fmt::format("Unable to create an e1.31 socket: {}",
                                                        getDetailedSocketError("e131_socket"));
            this->logger->critical(errorMessage);
            throw E131Exception(errorMessage);
        }
        logger->debug("Socket created successfully (fd: {})", sockfd);

        // Bind to the socket with detailed error reporting
        logger->debug("Binding to E1.31 port {}...", E131_DEFAULT_PORT);
        if (e131_bind(sockfd, E131_DEFAULT_PORT) < 0) {
            const std::string errorMessage = fmt::format("Unable to bind to the default e1.31 port {}: {}",
                                                        E131_DEFAULT_PORT,
                                                        getDetailedSocketError("e131_bind"));
            this->logger->critical(errorMessage);
            close(sockfd);
            throw E131Exception(errorMessage);
        }
        logger->debug("Socket bound successfully to port {}", E131_DEFAULT_PORT);

        // Log multicast join attempt with all details
        logger->info("Attempting to join multicast group:");
        logger->info("  Universe: {}", creature->getUniverse());
        logger->info("  Interface IP: {}", networkDeviceIPAddress);
        logger->info("  Socket FD: {}", sockfd);

        // Join the multicast group with enhanced error reporting
        if (e131_multicast_join_ifaddr(sockfd, creature->getUniverse(), this->networkDeviceIPAddress.c_str()) < 0) {
            // Get the detailed system error
            std::string systemError = getDetailedSocketError("e131_multicast_join_ifaddr");

            const std::string errorMessage = fmt::format(
                "Unable to join the multicast group for universe {} on interface {}. {}",
                creature->getUniverse(),
                this->networkDeviceIPAddress,
                systemError);

            this->logger->critical(errorMessage);

            close(sockfd);
            throw E131Exception(errorMessage);
        }

        logger->info("Successfully joined multicast group for universe {} on {}",
                    creature->getUniverse(), networkDeviceIPAddress);

        // loop to receive E1.31 packets
        this->logger->info("Waiting for E1.31 packets on network device {}", this->networkDeviceIPAddress);
        while (!stop_requested.load()) {

            if (e131_recv(sockfd, &packet) < 0) {
                std::string errorMessage = fmt::format("Unable to receive an e1.31 packet: {}",
                                                      getDetailedSocketError("e131_recv"));
                this->logger->critical(errorMessage);
                // Should we throw here?
                continue;
            }

            if ((error = e131_pkt_validate(&packet)) != E131_ERR_NONE) {
                logger->warn("E1.31 packet error: {}", e131_strerror(error));
                continue;
            }

            if (e131_pkt_discard(&packet, last_seq)) {
                logger->warn("E1.31 packet out of order received (seq: {}, last: {})",
                           packet.frame.seq_number, last_seq);
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
        for(u16 i = creature->getChannelOffset(); i < creature->getNumberOfServos() + creature->getChannelOffset(); i++) {
            hexString += fmt::format("{:#04x} ", packet.dmp.prop_val[i]);
        }
        logger->trace("Received e1.31 packet: {}", hexString);

        std::vector<Input> inputs;

        // Walk the input map
        for( auto&[fst, snd] : this->inputMap ) {
            const u16 slot = fst + creature->getChannelOffset();
            const u8 value = packet.dmp.prop_val[slot];

            auto inputToSend = Input(snd);
            inputToSend.setIncomingRequest(value);

            inputs.push_back(inputToSend);
            //logger->trace("Setting input slot {} ({}) to value {}", slot, inputToSend.getName(), value);
        }

        this->controller->acceptInput(inputs);
    }

} // namespace creatures::dmx

#pragma clang diagnostic pop
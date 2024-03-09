
#include <net/if.h>
#include <thread>
#include <unordered_map>

#include "e131.h"

#include "creature/Creature.h"
#include "dmx/E131Exception.h"
#include "dmx/E131Client.h"
#include "logging/Logger.h"
#include "util/thread_name.h"


#include "controller-config.h"


#pragma clang diagnostic push
#pragma ide diagnostic ignored "LoopDoesntUseConditionVariableInspection"
extern bool shouldRun;

namespace creatures::dmx {


    E131Client::E131Client(const std::shared_ptr<creatures::Logger>& logger) : logger(logger) {

        this->logger->info("e1.31 client created");

    }

    E131Client::~E131Client() {
        if(workerThread.joinable()) {
            workerThread.join(); // Clean up!
        }
    }

    void E131Client::init(const std::shared_ptr<creatures::creature::Creature>& _creature,
                          const std::shared_ptr<Controller>& _controller,
                          const char* _networkDeviceName) {
        this->creature = _creature;
        this->controller = _controller;
        this->networkDeviceName = _networkDeviceName;

        // Look up the network device index
        this->networkDeviceIndex = if_nametoindex(this->networkDeviceName.c_str());
        if( this->networkDeviceIndex  == 0 ) {
            std::string errorMessage = fmt::format("Unable to find network device index for name {}", this->networkDeviceName);
            logger->error(errorMessage);
            throw E131Exception(errorMessage);
        } else {
            logger->debug("Found network device index {} for name {}", this->networkDeviceIndex, this->networkDeviceName);
        }


        // Create our input map
        for( const auto& input : this->creature->getInputs() ) {

            // Make a new Input and copy the data into it
            auto newInput = creatures::Input(input);
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


        this->logger->info("✨e1.31 client started with {} inputs ✨", this->inputMap.size());

        workerThread = std::thread([this] {
            this->run();
        });

        workerThread.detach();

    }

    void E131Client::run() {

        setThreadName("E131Client::run");

        this->logger->info("e1.31 worker thread going");

        int sockfd;
        e131_packet_t packet;
        e131_error_t error;
        u8 last_seq = 0x00;

        // Create an e1.31 socket
        if ((sockfd = e131_socket()) < 0) {
            std::string errorMessage = "Unable to create an e1.31 socket";
            this->logger->critical(errorMessage);
            throw E131Exception(errorMessage);
        }

        // Bind to the socket ⛓️
        if (e131_bind(sockfd, E131_DEFAULT_PORT) < 0) {
            std::string errorMessage = "Unable to bind to the default e1.31 port";
            this->logger->critical(errorMessage);
            throw E131Exception(errorMessage);
        }

        // Join the multicast group for this creature's ACN universe
        if (e131_multicast_join_iface(sockfd, creature->getUniverse(), this->networkDeviceIndex) < 0) {
            std::string errorMessage = fmt::format("Unable to join the multicast group for universe {} on interface {} ({})",
                                                   creature->getUniverse(), this->networkDeviceIndex, this->networkDeviceName);
            this->logger->critical(errorMessage);
            throw E131Exception(errorMessage);
        }

        // loop to receive E1.31 packets
        this->logger->info("🕰️waiting for E1.31 packets on network device {}! ({})", this->networkDeviceIndex, this->networkDeviceName);
        while (shouldRun) {

            if (e131_recv(sockfd, &packet) < 0) {
                std::string errorMessage = "Unable to receive an e1.31 packet";
                this->logger->critical(errorMessage);

                // Should we throw here?
                continue;
            }
            if ((error = e131_pkt_validate(&packet)) != E131_ERR_NONE) {
                logger->warn("E1.31 packet error: {}", e131_strerror(error));
                continue;
            }
            if (e131_pkt_discard(&packet, last_seq)) {
                logger->warn("E1.31 packet out of order received");
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

        std::vector<creatures::Input> inputs;

        // Walk the input map
        for( auto& input : this->inputMap ) {

            u16 slot = input.first + creature->getChannelOffset();
            u8 value = packet.dmp.prop_val[slot];

            auto inputToSend = creatures::Input(input.second);
            inputToSend.setIncomingRequest(value);

            inputs.push_back(inputToSend);

            //logger->trace("Setting input slot {} ({})to value {}", slot, inputToSend.getName(), value);
        }

        this->controller->acceptInput(inputs);

    }

} // namespace creatures::dmx

#pragma clang diagnostic pop
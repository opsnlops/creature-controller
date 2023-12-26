
#include <thread>

#include "e131.h"

#include "dmx/E131Exception.h"
#include "dmx/E131Server.h"
#include "logging/Logger.h"
#include "util/thread_name.h"


#include "controller-config.h"




namespace creatures::dmx {

    E131Server::E131Server(const std::shared_ptr<creatures::Logger>& logger) : logger(logger) {

        this->logger->info("e1.31 server created");

    }

    E131Server::~E131Server() {
        if(workerThread.joinable()) {
            workerThread.join(); // Clean up!
        }
    }

    void E131Server::init(const std::shared_ptr<Creature>& _creature, const std::shared_ptr<Controller>& _controller) {
        this->creature = _creature;
        this->controller = _controller;

        logger->debug("e1.31 server init'ed");
    }



    void E131Server::start() {

        // Make sure we have our creature and controller
        if(this->creature == nullptr) {
            throw E131Exception("Unable to start e1.31 server without a creature");
        }
        if(this->controller == nullptr) {
            throw E131Exception("Unable to start e1.31 server without a controller");
        }


        this->logger->info("e1.31 server started");

        workerThread = std::thread([this] {
            this->run();
        });

        workerThread.detach();

    }

    [[noreturn]] void E131Server::run() {

        setThreadName("E131Server::run");

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

        // Join the multicast group for this creature's DMX universe
        if (e131_multicast_join_iface(sockfd, creature->getDmxUniverse(), 0) < 0) {
            std::string errorMessage = fmt::format("Unable to join the multicast group for universe {}",
                                                   creature->getDmxUniverse());
            this->logger->critical(errorMessage);
            throw E131Exception(errorMessage);
        }

        // loop to receive E1.31 packets
        this->logger->info("waiting for E1.31 packets!");
        for (EVER) {
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

    }

    void E131Server::handlePacket(const e131_packet_t &packet) {

        std::string hexString = "";

        for(u16 i = creature->getStartingDmxChannel(); i < 7; i++) {
            hexString += fmt::format("{:#04x} ", packet.dmp.prop_val[i]);
        }

        logger->info("Received e1.31 packet: {}", hexString);

    }

} // namespace creatures::dmx

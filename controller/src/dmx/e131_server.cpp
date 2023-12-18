
#include <thread>


#include "logging/Logger.h"
#include "util/thread_name.h"

#include "controller-config.h"
#include "e131_server.h"

#include "e131.h"

namespace creatures {

    E131Server::E131Server(const std::shared_ptr<creatures::Logger>& logger) : logger(logger) {

        this->logger->info("e1.31 server created");

    }

    E131Server::~E131Server() {
        if(workerThread.joinable()) {
            workerThread.join(); // Clean up!
        }
    }

    void E131Server::start() {

        this->logger->info("e1.31 server started");

        workerThread = std::thread([this] {
            this->run();
        });

        workerThread.detach();

    }

    [[noreturn]] void E131Server::run() {

        setThreadName("E131Server::run");

        this->logger->info("e1.31 worker thread going");


        // TODO: This is just the sample server code from libe131

        int sockfd;
        e131_packet_t packet;
        e131_error_t error;
        uint8_t last_seq = 0x00;

        // create a socket for E1.31
        if ((sockfd = e131_socket()) < 0)
            this->logger->critical("e131_socket");

        // bind the socket to the default E1.31 port
        if (e131_bind(sockfd, E131_DEFAULT_PORT) < 0)
            this->logger->critical( "e131_bind");

        // join the socket to multicast group for universe 1 on the default network interface
        if (e131_multicast_join_iface(sockfd, 1, 0) < 0)
            this->logger->critical( "e131_multicast_join_iface");

        // loop to receive E1.31 packets
        fprintf(stderr, "waiting for E1.31 packets ...\n");
        for (EVER) {
            if (e131_recv(sockfd, &packet) < 0)
                this->logger->critical("e131_recv");
            if ((error = e131_pkt_validate(&packet)) != E131_ERR_NONE) {
                fprintf(stderr, "e131_pkt_validate: %s\n", e131_strerror(error));
                continue;
            }
            if (e131_pkt_discard(&packet, last_seq)) {
                fprintf(stderr, "warning: packet out of order received\n");
                last_seq = packet.frame.seq_number;
                continue;
            }
            e131_pkt_dump(stdout, &packet);
            last_seq = packet.frame.seq_number;
        }

    }

}
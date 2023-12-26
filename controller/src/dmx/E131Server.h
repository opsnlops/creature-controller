#pragma once

#include <thread>

#include "controller/Controller.h"
#include "creature/Creature.h"
#include "logging/Logger.h"
#include "controller-config.h"


#include "e131.h"

namespace creatures::dmx {

    class E131Server {

    public:
        E131Server(const std::shared_ptr<creatures::Logger>& logger);
        ~E131Server();

        void init(const std::shared_ptr<Creature>& creature, const std::shared_ptr<Controller>& controller);
        void start();


    private:

        [[noreturn]] void run();
        std::thread workerThread;
        std::shared_ptr<creatures::Logger> logger;
        std::shared_ptr<Creature> creature;
        std::shared_ptr<Controller> controller;

        void handlePacket(const e131_packet_t & packet);

    };

} // creatures::dmx
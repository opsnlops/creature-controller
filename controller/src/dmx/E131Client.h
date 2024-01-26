#pragma once

#include <thread>

#include "controller/Controller.h"
#include "creature/Creature.h"
#include "logging/Logger.h"
#include "controller-config.h"


#include "e131.h"

namespace creatures::dmx {

    class E131Client {

    public:
        E131Client(const std::shared_ptr<creatures::Logger>& logger);
        ~E131Client();

        void init(const std::shared_ptr<creatures::creature::Creature>& creature,
                  const std::shared_ptr<Controller>& controller,
                  const int networkDevice);
        void start();


    private:

        void run();
        std::thread workerThread;
        std::shared_ptr<creatures::Logger> logger;
        std::shared_ptr<creatures::creature::Creature> creature;
        std::shared_ptr<Controller> controller;

        /**
         * A map of the inputs, with the slot number as the key. It's built in the
         * init() function.
         */
        std::unordered_map<u16, creatures::Input> inputMap;

        void handlePacket(const e131_packet_t & packet);


        int networkDevice = DEFAULT_NETWORK_DEVICE_NUMBER;
    };

} // creatures::dmx
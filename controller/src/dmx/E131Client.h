#pragma once

#include <atomic>
#include <thread>

#include "controller/Controller.h"
#include "creature/Creature.h"
#include "logging/Logger.h"
#include "controller-config.h"
#include "util/StoppableThread.h"


#include "e131.h"

namespace creatures::dmx {

    class E131Client : public StoppableThread {

    public:
        explicit E131Client(const std::shared_ptr<creatures::Logger>& logger);
        ~E131Client() override;

        void init(const std::shared_ptr<creatures::creature::Creature>& creature,
                  const std::shared_ptr<Controller>& controller,
                  std::string networkDeviceIPAddress);
        void start() override;
        void run() override;


    private:

        std::shared_ptr<creatures::Logger> logger;
        std::shared_ptr<creatures::creature::Creature> creature;
        std::shared_ptr<Controller> controller;

        /**
         * A map of the inputs, with the slot number as the key. It's built in the
         * init() function.
         */
        std::unordered_map<u16, creatures::Input> inputMap;

        void handlePacket(const e131_packet_t & packet);

        std::string networkDeviceIPAddress = DEFAULT_NETWORK_DEVICE_IP_ADDRESS;
    };

} // creatures::dmx
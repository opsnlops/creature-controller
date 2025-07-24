#pragma once

#include <thread>

#include "controller-config.h"
#include "controller/Controller.h"
#include "creature/Creature.h"
#include "logging/Logger.h"
#include "util/StoppableThread.h"

#include "e131.h"

namespace creatures::dmx {

class E131Client : public StoppableThread {

  public:
    explicit E131Client(const std::shared_ptr<Logger> &logger);
    ~E131Client() override;

    void init(const std::shared_ptr<creature::Creature> &creature, const std::shared_ptr<Controller> &controller,
              std::string networkInterfaceName, uint networkInterfaceIndex, std::string networkInterfaceAddress);
    void start() override;
    void run() override;

  private:
    std::shared_ptr<Logger> logger;
    std::shared_ptr<creature::Creature> creature;
    std::shared_ptr<Controller> controller;

    /**
     * A map of the inputs, with the slot number as the key. It's built in the
     * init() function.
     */
    std::unordered_map<u16, Input> inputMap;

    void handlePacket(const e131_packet_t &packet);

    std::string networkInterfaceName = DEFAULT_NETWORK_INTERFACE_NAME;
    std::string networkInterfaceAddress = DEFAULT_NETWORK_DEVICE_IP_ADDRESS;
    uint networkInterfaceIndex = 0;
};

} // namespace creatures::dmx
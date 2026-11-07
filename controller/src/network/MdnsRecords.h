#pragma once

#include <string>
#include <utility>
#include <vector>

#include "controller-config.h"
#include "creature/Creature.h"

namespace creatures::network {

// DNS-SD service type for a running creature controller
inline constexpr const char *CREATURE_CONTROLLER_SERVICE_TYPE = "_creature-controller._udp";

/**
 * An mDNS service to advertise: the instance name, type, port, and TXT records
 */
struct MdnsService {
    std::string name;
    std::string type;
    u16 port = 0;
    std::vector<std::pair<std::string, std::string>> txt;
};

/**
 * The mDNS instance name for a creature, which is its name in lower case
 */
std::string mdnsInstanceName(const std::string &creatureName);

/**
 * Describe a running creature for mDNS
 *
 * @param creature the creature this controller is running
 * @param universe the E1.31 universe the controller listens on
 * @param controllerVersion this controller's version
 * @param port the E1.31 port the controller listens on
 */
MdnsService makeCreatureService(const creature::Creature &creature, u16 universe, const std::string &controllerVersion,
                                u16 port);

} // namespace creatures::network

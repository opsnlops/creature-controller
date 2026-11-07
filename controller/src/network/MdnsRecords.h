#pragma once

#include <string>

#include "controller-config.h"
#include "creature/Creature.h"
#include "network/MdnsService.h"

namespace creatures::network {

// DNS-SD service type for a running creature controller
inline constexpr const char *CREATURE_CONTROLLER_SERVICE_TYPE = "_creature-controller._udp";

/**
 * Details about the controller and its host that go into the advertisement
 */
struct ControllerDetails {
    std::string controllerVersion;
    u16 port = 0; // The E1.31 port the controller listens on
    u16 universe = 0;
    std::string networkInterface;
    std::string serialDevice;
    bool rtpAudio = false;
    std::string audioDevice;
    std::string server; // "address:port", or "off"
    double temperatureLimitF = 0.0;
    u32 firmwareVersion = 0; // Zero until the firmware reports in
};

/**
 * Describe a running creature for mDNS
 */
MdnsService makeCreatureService(const creature::Creature &creature, const ControllerDetails &details);

} // namespace creatures::network

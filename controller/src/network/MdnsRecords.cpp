#include <algorithm>
#include <cctype>
#include <string>

#include "network/MdnsRecords.h"

namespace creatures::network {

std::string mdnsInstanceName(const std::string &creatureName) {
    std::string name = creatureName;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}

MdnsService makeCreatureService(const creature::Creature &creature, u16 universe, const std::string &controllerVersion,
                                u16 port) {
    MdnsService service;
    service.name = mdnsInstanceName(creature.getName());
    service.type = CREATURE_CONTROLLER_SERVICE_TYPE;
    service.port = port;
    service.txt = {
        {"txtvers", "1"},
        {"id", creature.getId()},
        {"name", creature.getName()},
        {"type", creature::Creature::creatureTypeToString(creature.getType())},
        {"version", creature.getVersion()},
        {"universe", std::to_string(universe)},
        {"channel_offset", std::to_string(creature.getChannelOffset())},
        {"audio_channel", std::to_string(creature.getAudioChannel())},
        {"controller_version", controllerVersion},
    };
    return service;
}

} // namespace creatures::network

#include <string>

#include <fmt/format.h>

#include "network/MdnsRecords.h"

namespace creatures::network {

namespace {
std::string join(const std::vector<std::string> &items) {
    std::string joined;
    for (const auto &item : items) {
        joined += (joined.empty() ? "" : ",") + item;
    }
    return joined;
}
} // namespace

MdnsService makeCreatureService(const creature::Creature &creature, const ControllerDetails &details) {
    MdnsService service;
    service.name = mdnsInstanceName(creature.getName());
    service.type = CREATURE_CONTROLLER_SERVICE_TYPE;
    service.port = details.port;

    const auto motorIds = creature.getMotorIds();
    service.txt = {
        {"txtvers", "1"},
        {"id", creature.getId()},
        {"name", creature.getName()},
        {"type", creature::Creature::creatureTypeToString(creature.getType())},
        {"version", creature.getVersion()},
        {"description", creature.getDescription()},
        {"controller_version", details.controllerVersion},
        {"firmware_version", details.firmwareVersion ? std::to_string(details.firmwareVersion) : "unknown"},
        {"universe", std::to_string(details.universe)},
        {"channel_offset", std::to_string(creature.getChannelOffset())},
        {"audio_channel", std::to_string(creature.getAudioChannel())},
        {"mouth_slot", std::to_string(creature.getMouthSlot())},
        {"inputs", std::to_string(creature.getInputs().size())},
        {"motors", std::to_string(motorIds.size())},
        {"motor_ids", join(motorIds)},
        {"servo_hz", std::to_string(creature.getServoUpdateFrequencyHz())},
        {"position_min", std::to_string(creature.getPositionMin())},
        {"position_max", std::to_string(creature.getPositionMax())},
        {"power_limit_w", fmt::format("{:g}", creature.getPowerDrawLimitWatts())},
        {"power_warning_w", fmt::format("{:g}", creature.getPowerDrawWarningWatts())},
        {"power_response_s", fmt::format("{:g}", creature.getPowerDrawResponseSeconds())},
        {"temp_limit_f", fmt::format("{:g}", details.temperatureLimitF)},
        {"serial_device", details.serialDevice},
        {"interface", details.networkInterface},
        {"rtp_audio", details.rtpAudio ? "on" : "off"},
        {"audio_device", details.audioDevice},
        {"server", details.server},
    };
    return service;
}

} // namespace creatures::network

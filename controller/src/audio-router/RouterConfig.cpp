#include "audio-router/RouterConfig.h"
#include <algorithm>
#include <arpa/inet.h>
#include <fstream>
#include <ifaddrs.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace creatures::audio::router {
namespace {
using Json = nlohmann::json;
void fields(const Json &object, std::initializer_list<const char *> allowed) {
    if (!object.is_object())
        throw std::invalid_argument("Configuration section must be an object");
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (std::find(allowed.begin(), allowed.end(), it.key()) == allowed.end())
            throw std::invalid_argument("Unknown configuration field: " + it.key());
    }
}
int integer(const Json &object, const char *name, int minimum, int maximum, int fallback) {
    if (!object.contains(name))
        return fallback;
    const auto &value = object.at(name);
    if (!value.is_number_integer() || value < minimum || value > maximum)
        throw std::invalid_argument(std::string(name) + " is outside its integer range");
    return value.get<int>();
}
size_t inputIndex(const AudioEngineConfig &config, int channel) {
    for (size_t i = 0; i < config.inputs.size(); ++i)
        if (config.inputs[i].channel == channel)
            return i;
    throw std::invalid_argument("Route refers to an unsubscribed input channel");
}
size_t outputIndex(const AudioEngineConfig &config, const std::string &name) {
    const auto speaker = parseSpeakerChannel(name);
    for (size_t i = 0; i < config.output.channels.size(); ++i)
        if (config.output.channels[i] == speaker)
            return i;
    throw std::invalid_argument("Route refers to an unconfigured output channel: " + name);
}
const Json &array(const Json &object, const char *name) {
    const auto &value = object.at(name);
    if (!value.is_array())
        throw std::invalid_argument(std::string(name) + " must be an array");
    return value;
}
} // namespace
RouterConfig parseRouterConfig(const Json &json) {
    fields(json, {"networkInterface", "logLevel", "rtpPort", "requireRtcp", "output", "inputs", "routes", "bass"});
    RouterConfig router;
    router.networkInterface = json.at("networkInterface").get<std::string>();
    if (router.networkInterface.empty())
        throw std::invalid_argument("networkInterface must not be empty");
    router.logLevel = json.value("logLevel", std::string("info"));
    if (router.logLevel != "trace" && router.logLevel != "debug" && router.logLevel != "info" &&
        router.logLevel != "warn" && router.logLevel != "error" && router.logLevel != "critical" &&
        router.logLevel != "off")
        throw std::invalid_argument("Invalid logLevel");
    auto &config = router.engine;
    config.interfaceIp = "0.0.0.0"; // Resolved at startup, so offline config checks do not depend on host interfaces.
    config.port = static_cast<uint16_t>(integer(json, "rtpPort", 1, 65534, RTP_PORT));
    config.requireRtcp = json.value("requireRtcp", true);
    const auto &output = json.at("output");
    fields(output, {"deviceName", "channels", "commonPlayoutDelayMs", "audioDeviceCompensationMs", "limiterCeilingDb",
                    "outputVolumePercent", "alsaMixerCard", "alsaMixerElement"});
    config.output.deviceName = output.at("deviceName").get<std::string>();
    config.output.channels.clear();
    for (const auto &name : array(output, "channels"))
        config.output.channels.push_back(parseSpeakerChannel(name.get<std::string>()));
    config.output.commonPlayoutDelayMs =
        static_cast<uint16_t>(integer(output, "commonPlayoutDelayMs", MIN_COMMON_PLAYOUT_DELAY_MS,
                                      MAX_COMMON_PLAYOUT_DELAY_MS, DEFAULT_COMMON_PLAYOUT_DELAY_MS));
    config.output.audioDeviceCompensationMs =
        static_cast<int16_t>(integer(output, "audioDeviceCompensationMs", -MAX_ABSOLUTE_DEVICE_COMPENSATION_MS,
                                     MAX_ABSOLUTE_DEVICE_COMPENSATION_MS, 0));
    config.output.limiterCeilingDb = output.value("limiterCeilingDb", DEFAULT_LIMITER_CEILING_DB);
    if (output.contains("outputVolumePercent"))
        config.output.outputVolumePercent = static_cast<uint8_t>(integer(output, "outputVolumePercent", 0, 100, 0));
    config.output.alsaMixerCard = output.value("alsaMixerCard", std::string("default"));
    config.output.alsaMixerElement = output.value("alsaMixerElement", std::string{});
    for (const auto &input : array(json, "inputs")) {
        fields(input, {"channel", "gainDb"});
        config.inputs.push_back(
            {static_cast<uint8_t>(integer(input, "channel", 1, 17, 0)), input.value("gainDb", 0.0f)});
    }
    for (const auto &route : array(json, "routes")) {
        fields(route, {"input", "output", "gainDb"});
        config.routes.push_back({inputIndex(config, integer(route, "input", 1, 17, 0)),
                                 outputIndex(config, route.at("output").get<std::string>()),
                                 route.value("gainDb", 0.0f)});
    }
    if (json.contains("bass")) {
        const auto &bass = json.at("bass");
        fields(bass, {"output", "crossoverHz", "gainDb", "invertPolarity", "inputs", "highPassOutputs"});
        BassConfig bassConfig;
        bassConfig.output = outputIndex(config, bass.at("output").get<std::string>());
        bassConfig.crossoverHz = bass.value("crossoverHz", 80.0f);
        bassConfig.gainDb = bass.value("gainDb", 0.0f);
        bassConfig.invertPolarity = bass.value("invertPolarity", false);
        for (const auto &send : array(bass, "inputs")) {
            fields(send, {"channel", "gainDb"});
            bassConfig.sends.push_back(
                {inputIndex(config, integer(send, "channel", 1, 17, 0)), send.value("gainDb", 0.0f)});
        }
        if (bass.contains("highPassOutputs"))
            for (const auto &name : array(bass, "highPassOutputs"))
                bassConfig.highPassOutputs.push_back(outputIndex(config, name.get<std::string>()));
        config.bass = std::move(bassConfig);
    }
    validateAudioEngineConfig(config);
    return router;
}
RouterConfig loadRouterConfig(const std::string &path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Unable to open router configuration: " + path);
    return parseRouterConfig(Json::parse(input));
}
std::string resolveInterfaceAddress(const std::string &interfaceName) {
    in_addr address{};
    if (inet_pton(AF_INET, interfaceName.c_str(), &address) == 1)
        return interfaceName;
    ifaddrs *raw = nullptr;
    if (getifaddrs(&raw) != 0)
        throw std::runtime_error("Unable to enumerate network interfaces");
    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> interfaces(raw, &freeifaddrs);
    std::string result;
    for (auto *iface = raw; iface; iface = iface->ifa_next) {
        if (!iface->ifa_addr || iface->ifa_addr->sa_family != AF_INET || interfaceName != iface->ifa_name)
            continue;
        const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(iface->ifa_addr);
        char text[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text)))
            continue;
        if (!result.empty() && result != text)
            throw std::runtime_error("Network interface has multiple IPv4 addresses; configure an explicit address");
        result = text;
    }
    if (result.empty())
        throw std::runtime_error("Network interface has no IPv4 address: " + interfaceName);
    return result;
}
} // namespace creatures::audio::router

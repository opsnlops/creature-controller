#include <string>

#include <fmt/format.h>

#include "audio-router/RouterMdns.h"

namespace creatures::audio::router {

namespace {
void append(std::string &list, const std::string &item) { list += (list.empty() ? "" : ",") + item; }

std::string outputName(const AudioEngineConfig &engine, size_t output) {
    return output < engine.output.channels.size() ? speakerChannelName(engine.output.channels[output])
                                                  : std::to_string(output);
}

std::string inputChannel(const AudioEngineConfig &engine, size_t input) {
    return input < engine.inputs.size() ? std::to_string(engine.inputs[input].channel) : std::to_string(input);
}
} // namespace

network::MdnsService makeRouterService(const std::string &name, const RouterConfig &config,
                                       const std::string &routerVersion) {
    const auto &engine = config.engine;

    std::string outputs;
    for (const auto channel : engine.output.channels) {
        append(outputs, speakerChannelName(channel));
    }

    std::string inputs;
    std::string inputGains;
    for (const auto &input : engine.inputs) {
        append(inputs, std::to_string(input.channel));
        append(inputGains, fmt::format("{:g}", input.gainDb));
    }

    // Routes read as input channel>speaker, with a gain only when there is one
    std::string routes;
    for (const auto &route : engine.routes) {
        std::string entry = inputChannel(engine, route.input) + ">" + outputName(engine, route.output);
        if (route.gainDb != 0.0f) {
            entry += fmt::format("@{:g}", route.gainDb);
        }
        append(routes, entry);
    }

    std::string bass = "off";
    if (engine.bass) {
        bass = fmt::format("{}@{:g}Hz", outputName(engine, engine.bass->output), engine.bass->crossoverHz);
    }

    const std::string device = engine.output.deviceName.value_or(fmt::format("device {}", engine.output.deviceNumber));

    network::MdnsService service;
    service.name = network::mdnsInstanceName(name);
    service.type = AUDIO_ROUTER_SERVICE_TYPE;
    service.port = engine.port;
    service.txt = {
        {"txtvers", "1"},
        {"version", routerVersion},
        {"interface", config.networkInterface},
        {"device", device},
        {"outputs", outputs},
        {"inputs", inputs},
        {"input_gains_db", inputGains},
        {"routes", routes},
        {"bass", bass},
        {"playout_delay_ms", std::to_string(engine.output.commonPlayoutDelayMs)},
        {"device_compensation_ms", std::to_string(engine.output.audioDeviceCompensationMs)},
        {"limiter_ceiling_db", fmt::format("{:g}", engine.output.limiterCeilingDb)},
        {"require_rtcp", engine.requireRtcp ? "yes" : "no"},
    };
    return service;
}

} // namespace creatures::audio::router

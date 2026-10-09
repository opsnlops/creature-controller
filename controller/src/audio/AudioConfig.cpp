#include "audio/audio-config.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cmath>
#include <set>
#include <stdexcept>

namespace creatures::audio {
const char *speakerChannelName(SpeakerChannel channel) {
    switch (channel) {
    case SpeakerChannel::Mono:
        return "mono";
    case SpeakerChannel::FrontLeft:
        return "front_left";
    case SpeakerChannel::FrontRight:
        return "front_right";
    case SpeakerChannel::Center:
        return "center";
    case SpeakerChannel::Lfe:
        return "lfe";
    case SpeakerChannel::RearLeft:
        return "rear_left";
    case SpeakerChannel::RearRight:
        return "rear_right";
    case SpeakerChannel::SideLeft:
        return "side_left";
    case SpeakerChannel::SideRight:
        return "side_right";
    }
    throw std::invalid_argument("Invalid speaker channel");
}

SpeakerChannel parseSpeakerChannel(const std::string &name) {
    for (auto channel : {SpeakerChannel::Mono, SpeakerChannel::FrontLeft, SpeakerChannel::FrontRight,
                         SpeakerChannel::Center, SpeakerChannel::Lfe, SpeakerChannel::RearLeft,
                         SpeakerChannel::RearRight, SpeakerChannel::SideLeft, SpeakerChannel::SideRight}) {
        if (name == speakerChannelName(channel))
            return channel;
    }
    throw std::invalid_argument("Unknown speaker channel: " + name);
}

namespace {
void validateGain(float gain) {
    if (!std::isfinite(gain) || gain < MIN_GAIN_DB || gain > MAX_GAIN_DB)
        throw std::invalid_argument("Audio gain must be finite and between -90 and +12 dB");
}
} // namespace

void validateAudioEngineConfig(const AudioEngineConfig &config) {
    in_addr address{};
    if (inet_pton(AF_INET, config.interfaceIp.c_str(), &address) != 1)
        throw std::invalid_argument("Audio interface must be an IPv4 address");
    if (config.port == 0 || (config.requireRtcp && config.port == 65535))
        throw std::invalid_argument("Invalid RTP port (RTCP needs the adjacent port)");
    const auto &output = config.output;
    if (output.channels.empty() || output.channels.size() > 8)
        throw std::invalid_argument("Audio output requires between one and eight channels");
    std::set<SpeakerChannel> speakers;
    for (auto channel : output.channels) {
        (void)speakerChannelName(channel);
        if (!speakers.insert(channel).second || (channel == SpeakerChannel::Mono && output.channels.size() != 1))
            throw std::invalid_argument("Output speaker channels must be unique; mono cannot be combined");
    }
    if (output.deviceName && output.deviceName->empty())
        throw std::invalid_argument("Empty audio device name");
    if (!std::isfinite(output.limiterCeilingDb) || output.limiterCeilingDb < MIN_GAIN_DB || output.limiterCeilingDb > 0)
        throw std::invalid_argument("Invalid limiter ceiling");
    if (output.outputVolumePercent && *output.outputVolumePercent > 100)
        throw std::invalid_argument("Invalid output volume");
    const int minimumDelay = FRAME_MS + RTCP_ENQUEUE_HEADROOM_MS + std::max<int>(0, output.audioDeviceCompensationMs);
    if (output.commonPlayoutDelayMs < MIN_COMMON_PLAYOUT_DELAY_MS ||
        output.commonPlayoutDelayMs > MAX_COMMON_PLAYOUT_DELAY_MS || output.commonPlayoutDelayMs < minimumDelay ||
        std::abs(output.audioDeviceCompensationMs) > MAX_ABSOLUTE_DEVICE_COMPENSATION_MS)
        throw std::invalid_argument("Invalid playout delay or device compensation");
    std::set<uint8_t> inputs;
    for (const auto &input : config.inputs) {
        if (input.channel < 1 || input.channel > 17 || !inputs.insert(input.channel).second)
            throw std::invalid_argument("Input channels must be unique and between 1 and 17");
        validateGain(input.gainDb);
    }
    if (!inputs.contains(17))
        throw std::invalid_argument("BGM channel 17 is required as the timing master");
    std::set<std::pair<size_t, size_t>> routes;
    for (const auto &route : config.routes) {
        if (route.input >= config.inputs.size() || route.output >= output.channels.size() ||
            !routes.emplace(route.input, route.output).second)
            throw std::invalid_argument("Invalid or duplicate audio route");
        if (config.bass && route.output == config.bass->output)
            throw std::invalid_argument("The filtered bass output cannot also receive direct routes");
        validateGain(route.gainDb);
    }
    if (config.bass) {
        const auto &bass = *config.bass;
        if (bass.output >= output.channels.size() || output.channels[bass.output] != SpeakerChannel::Lfe ||
            !std::isfinite(bass.crossoverHz) || bass.crossoverHz < 20 || bass.crossoverHz > 200 || bass.sends.empty())
            throw std::invalid_argument("Bass requires an LFE output, input sends, and a 20-200 Hz crossover");
        validateGain(bass.gainDb);
        std::set<size_t> sends, highPass;
        for (const auto &send : bass.sends) {
            if (send.input >= config.inputs.size() || !sends.insert(send.input).second)
                throw std::invalid_argument("Invalid or duplicate bass send");
            validateGain(send.gainDb);
        }
        for (size_t channel : bass.highPassOutputs) {
            if (channel >= output.channels.size() || channel == bass.output || !highPass.insert(channel).second)
                throw std::invalid_argument("Invalid or duplicate high-pass output");
        }
    }
}

AudioEngineConfig creatureAudioConfig(uint8_t channel, const std::string &interfaceIp, const AudioConfig &output,
                                      uint16_t port) {
    if (channel < 1 || channel > 16)
        throw std::invalid_argument("Creature audio channel must be 1-16");
    AudioEngineConfig config;
    config.interfaceIp = interfaceIp;
    config.port = port;
    config.output = output;
    config.inputs = {{channel, output.dialogGainDb}, {17, output.bgmGainDb}};
    config.routes = {{0, 0, 0}, {1, 0, 0}};
    validateAudioEngineConfig(config);
    return config;
}
} // namespace creatures::audio

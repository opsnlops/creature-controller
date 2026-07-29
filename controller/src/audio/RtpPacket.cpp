#include "audio/RtpPacket.h"

#include <cstddef>

#include "audio/audio-config.h"

namespace creatures::audio {
namespace {

uint16_t readNetworkU16(const uint8_t *bytes) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8U) | static_cast<uint16_t>(bytes[1]));
}

uint32_t readNetworkU32(const uint8_t *bytes) {
    return (static_cast<uint32_t>(bytes[0]) << 24U) | (static_cast<uint32_t>(bytes[1]) << 16U) |
           (static_cast<uint32_t>(bytes[2]) << 8U) | static_cast<uint32_t>(bytes[3]);
}

} // namespace

std::optional<RtpPacket> parseOpusRtpPacket(std::span<const uint8_t> packet) {
    constexpr size_t fixedHeaderSize = 12;
    if (packet.size() < fixedHeaderSize || ((packet[0] >> 6U) & 0x03U) != 2U) {
        return std::nullopt;
    }

    const bool hasPadding = (packet[0] & 0x20U) != 0;
    const bool hasExtension = (packet[0] & 0x10U) != 0;
    const size_t csrcCount = packet[0] & 0x0FU;
    const uint8_t payloadType = packet[1] & 0x7FU;
    if (payloadType != RTP_OPUS_PAYLOAD_TYPE) {
        return std::nullopt;
    }

    size_t payloadOffset = fixedHeaderSize + csrcCount * sizeof(uint32_t);
    if (payloadOffset > packet.size()) {
        return std::nullopt;
    }

    if (hasExtension) {
        constexpr size_t extensionHeaderSize = 4;
        if (payloadOffset + extensionHeaderSize > packet.size()) {
            return std::nullopt;
        }
        const size_t extensionWords = readNetworkU16(packet.data() + payloadOffset + 2);
        const size_t extensionSize = extensionHeaderSize + extensionWords * sizeof(uint32_t);
        if (payloadOffset + extensionSize > packet.size()) {
            return std::nullopt;
        }
        payloadOffset += extensionSize;
    }

    size_t payloadEnd = packet.size();
    if (hasPadding) {
        const size_t paddingBytes = packet.back();
        if (paddingBytes == 0 || paddingBytes > payloadEnd - payloadOffset) {
            return std::nullopt;
        }
        payloadEnd -= paddingBytes;
    }
    if (payloadOffset >= payloadEnd) {
        return std::nullopt;
    }

    RtpPacket parsed;
    parsed.sequenceNumber = readNetworkU16(packet.data() + 2);
    parsed.timestamp = readNetworkU32(packet.data() + 4);
    parsed.synchronizationSource = readNetworkU32(packet.data() + 8);
    parsed.payload.assign(packet.begin() + static_cast<std::ptrdiff_t>(payloadOffset),
                          packet.begin() + static_cast<std::ptrdiff_t>(payloadEnd));
    return parsed;
}

} // namespace creatures::audio

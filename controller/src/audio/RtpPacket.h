#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace creatures::audio {

struct RtpPacket {
    uint16_t sequenceNumber{0};
    uint32_t timestamp{0};
    uint32_t synchronizationSource{0};
    std::vector<uint8_t> payload;
};

[[nodiscard]] std::optional<RtpPacket> parseOpusRtpPacket(std::span<const uint8_t> packet);

} // namespace creatures::audio

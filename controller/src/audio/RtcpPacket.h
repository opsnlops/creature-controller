#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace creatures::audio {

struct RtcpSenderReport {
    uint32_t synchronizationSource{0};
    uint64_t ntpTimestamp{0};
    uint32_t rtpTimestamp{0};
    uint32_t packetCount{0};
    uint32_t octetCount{0};
    std::string canonicalName;
};

/**
 * Parse one RFC 3550 compound RTCP packet containing a Sender Report and an
 * SDES CNAME for the same SSRC.
 *
 * Unknown RTCP packet types are skipped. Malformed lengths, padding, report
 * blocks, and SDES chunks reject the complete datagram.
 */
[[nodiscard]] std::optional<RtcpSenderReport> parseRtcpSenderReport(std::span<const uint8_t> packet) noexcept;

} // namespace creatures::audio

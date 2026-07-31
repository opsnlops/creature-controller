#include "audio/RtcpPacket.h"

#include <cstddef>
#include <limits>

#include "audio/audio-config.h"

namespace creatures::audio {
namespace {

constexpr uint8_t RTCP_VERSION = 2;
constexpr uint8_t RTCP_SENDER_REPORT = 200;
constexpr uint8_t RTCP_SOURCE_DESCRIPTION = 202;
constexpr uint8_t RTCP_SDES_END = 0;
constexpr uint8_t RTCP_SDES_CNAME = 1;
constexpr size_t RTCP_HEADER_SIZE = 4;
constexpr size_t RTCP_SENDER_REPORT_FIXED_SIZE = 28;
constexpr size_t RTCP_REPORT_BLOCK_SIZE = 24;

uint16_t readNetworkU16(const uint8_t *bytes) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8U) | static_cast<uint16_t>(bytes[1]));
}

uint32_t readNetworkU32(const uint8_t *bytes) {
    return (static_cast<uint32_t>(bytes[0]) << 24U) | (static_cast<uint32_t>(bytes[1]) << 16U) |
           (static_cast<uint32_t>(bytes[2]) << 8U) | static_cast<uint32_t>(bytes[3]);
}

size_t alignToWord(size_t size) { return (size + sizeof(uint32_t) - 1U) & ~(sizeof(uint32_t) - 1U); }

bool allZero(std::span<const uint8_t> bytes) {
    for (const uint8_t byte : bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

bool parseSourceDescriptions(std::span<const uint8_t> block, uint8_t sourceCount, uint32_t senderSource,
                             std::optional<std::string> &canonicalName) {
    size_t offset = RTCP_HEADER_SIZE;
    for (uint8_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) {
        if (offset + sizeof(uint32_t) > block.size()) {
            return false;
        }

        const size_t chunkStart = offset;
        const uint32_t chunkSource = readNetworkU32(block.data() + offset);
        offset += sizeof(uint32_t);
        bool foundEnd = false;

        while (offset < block.size()) {
            const uint8_t itemType = block[offset++];
            if (itemType == RTCP_SDES_END) {
                foundEnd = true;
                break;
            }
            if (offset >= block.size()) {
                return false;
            }

            const size_t itemLength = block[offset++];
            if (itemLength > block.size() - offset) {
                return false;
            }

            if (itemType == RTCP_SDES_CNAME && chunkSource == senderSource) {
                if (itemLength == 0 || canonicalName.has_value()) {
                    return false;
                }
                canonicalName = std::string(reinterpret_cast<const char *>(block.data() + offset), itemLength);
            }
            offset += itemLength;
        }

        if (!foundEnd) {
            return false;
        }

        const size_t paddedChunkSize = alignToWord(offset - chunkStart);
        if (paddedChunkSize > block.size() - chunkStart) {
            return false;
        }
        const size_t paddedEnd = chunkStart + paddedChunkSize;
        if (!allZero(block.subspan(offset, paddedEnd - offset))) {
            return false;
        }
        offset = paddedEnd;
    }

    return allZero(block.subspan(offset));
}

} // namespace

std::optional<RtcpSenderReport> parseRtcpSenderReport(std::span<const uint8_t> packet) noexcept {
    try {
        if (packet.size() < RTCP_HEADER_SIZE || packet.size() > MAX_RTCP_PACKET_SIZE ||
            packet.size() % sizeof(uint32_t) != 0) {
            return std::nullopt;
        }

        std::optional<RtcpSenderReport> senderReport;
        std::optional<std::string> canonicalName;
        size_t offset = 0;
        size_t packetIndex = 0;

        while (offset < packet.size()) {
            if (packet.size() - offset < RTCP_HEADER_SIZE) {
                return std::nullopt;
            }

            const uint8_t firstByte = packet[offset];
            const uint8_t version = firstByte >> 6U;
            const bool hasPadding = (firstByte & 0x20U) != 0;
            const uint8_t reportCount = firstByte & 0x1FU;
            const uint8_t packetType = packet[offset + 1];
            const size_t lengthWords = readNetworkU16(packet.data() + offset + 2);
            if (lengthWords > (std::numeric_limits<size_t>::max() / sizeof(uint32_t)) - 1U) {
                return std::nullopt;
            }
            const size_t blockSize = (lengthWords + 1U) * sizeof(uint32_t);
            if (version != RTCP_VERSION || blockSize < RTCP_HEADER_SIZE || blockSize > packet.size() - offset) {
                return std::nullopt;
            }
            if (packetIndex == 0 && packetType != RTCP_SENDER_REPORT) {
                return std::nullopt;
            }

            size_t contentSize = blockSize;
            if (hasPadding) {
                if (offset + blockSize != packet.size()) {
                    return std::nullopt;
                }
                const size_t paddingSize = packet[offset + blockSize - 1U];
                if (paddingSize == 0 || paddingSize > blockSize - RTCP_HEADER_SIZE) {
                    return std::nullopt;
                }
                contentSize -= paddingSize;
            }
            const auto block = packet.subspan(offset, contentSize);

            if (packetType == RTCP_SENDER_REPORT) {
                const size_t requiredSize =
                    RTCP_SENDER_REPORT_FIXED_SIZE + static_cast<size_t>(reportCount) * RTCP_REPORT_BLOCK_SIZE;
                if (senderReport.has_value() || contentSize != requiredSize) {
                    return std::nullopt;
                }

                senderReport = RtcpSenderReport{
                    .synchronizationSource = readNetworkU32(block.data() + 4),
                    .ntpTimestamp = (static_cast<uint64_t>(readNetworkU32(block.data() + 8)) << 32U) |
                                    readNetworkU32(block.data() + 12),
                    .rtpTimestamp = readNetworkU32(block.data() + 16),
                    .packetCount = readNetworkU32(block.data() + 20),
                    .octetCount = readNetworkU32(block.data() + 24),
                    .canonicalName = {},
                };
                if (senderReport->synchronizationSource == 0) {
                    return std::nullopt;
                }
            } else if (packetType == RTCP_SOURCE_DESCRIPTION) {
                if (!senderReport.has_value() ||
                    !parseSourceDescriptions(block, reportCount, senderReport->synchronizationSource, canonicalName)) {
                    return std::nullopt;
                }
            }

            offset += blockSize;
            ++packetIndex;
        }

        if (!senderReport.has_value() || !canonicalName.has_value()) {
            return std::nullopt;
        }
        senderReport->canonicalName = std::move(*canonicalName);
        return senderReport;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace creatures::audio

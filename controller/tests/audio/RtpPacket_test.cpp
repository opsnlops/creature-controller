#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "audio/RtpPacket.h"
#include "audio/audio-config.h"

namespace creatures::audio {
namespace {

std::vector<uint8_t> makePacket(uint8_t firstByte = 0x80, uint8_t payloadType = RTP_OPUS_PAYLOAD_TYPE) {
    return {
        firstByte, payloadType, 0x12, 0x34, 0x01, 0x02, 0x03, 0x04, 0x10, 0x20, 0x30, 0x40, 0xAA, 0xBB,
    };
}

TEST(RtpPacketTest, ParsesFixedHeaderAndPayload) {
    const auto packet = parseOpusRtpPacket(makePacket());

    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(packet->sequenceNumber, 0x1234);
    EXPECT_EQ(packet->timestamp, 0x01020304U);
    EXPECT_EQ(packet->synchronizationSource, 0x10203040U);
    EXPECT_EQ(packet->payload, (std::vector<uint8_t>{0xAA, 0xBB}));
}

TEST(RtpPacketTest, SkipsCsrcAndHeaderExtension) {
    auto bytes = makePacket(0x91);
    bytes.insert(bytes.begin() + 12, {0x55, 0x66, 0x77, 0x88});
    bytes.insert(bytes.begin() + 16, {0xBE, 0xDE, 0x00, 0x01});
    bytes.insert(bytes.begin() + 20, {0x11, 0x22, 0x33, 0x44});

    const auto packet = parseOpusRtpPacket(bytes);

    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(packet->payload, (std::vector<uint8_t>{0xAA, 0xBB}));
}

TEST(RtpPacketTest, RemovesRtpPadding) {
    auto bytes = makePacket(0xA0);
    bytes.push_back(0x00);
    bytes.push_back(0x02);

    const auto packet = parseOpusRtpPacket(bytes);

    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(packet->payload, (std::vector<uint8_t>{0xAA, 0xBB}));
}

TEST(RtpPacketTest, RejectsMalformedOrUnexpectedPackets) {
    EXPECT_FALSE(parseOpusRtpPacket(std::array<uint8_t, 3>{0x80, RTP_OPUS_PAYLOAD_TYPE, 0}).has_value());
    EXPECT_FALSE(parseOpusRtpPacket(makePacket(0x40)).has_value());
    EXPECT_FALSE(parseOpusRtpPacket(makePacket(0x80, RTP_OPUS_PAYLOAD_TYPE + 1)).has_value());

    auto invalidExtension = makePacket(0x90);
    EXPECT_FALSE(parseOpusRtpPacket(invalidExtension).has_value());

    auto invalidPadding = makePacket(0xA0);
    invalidPadding.back() = 0x7F;
    EXPECT_FALSE(parseOpusRtpPacket(invalidPadding).has_value());
}

} // namespace
} // namespace creatures::audio

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "audio/RtcpPacket.h"
#include "audio/audio-config.h"

namespace creatures::audio {
namespace {

void appendU16(std::vector<uint8_t> &packet, uint16_t value) {
    packet.push_back(static_cast<uint8_t>(value >> 8U));
    packet.push_back(static_cast<uint8_t>(value));
}

void appendU32(std::vector<uint8_t> &packet, uint32_t value) {
    packet.push_back(static_cast<uint8_t>(value >> 24U));
    packet.push_back(static_cast<uint8_t>(value >> 16U));
    packet.push_back(static_cast<uint8_t>(value >> 8U));
    packet.push_back(static_cast<uint8_t>(value));
}

std::vector<uint8_t> makeServerCompoundPacket(uint32_t synchronizationSource = 0x0102'0304U,
                                              const std::string &canonicalName = "creature-server@test") {
    std::vector<uint8_t> packet{
        0x80,
        200,
        0x00,
        0x06,
    };
    appendU32(packet, synchronizationSource);
    appendU32(packet, 0x1122'3344U);
    appendU32(packet, 0x5566'7788U);
    appendU32(packet, 0x99aa'bbccU);
    appendU32(packet, 123U);
    appendU32(packet, 45'678U);

    const size_t sdesStart = packet.size();
    packet.push_back(0x81);
    packet.push_back(202);
    appendU16(packet, 0);
    appendU32(packet, synchronizationSource);
    packet.push_back(1);
    packet.push_back(static_cast<uint8_t>(canonicalName.size()));
    packet.insert(packet.end(), canonicalName.begin(), canonicalName.end());
    packet.push_back(0);
    while ((packet.size() - sdesStart) % sizeof(uint32_t) != 0) {
        packet.push_back(0);
    }
    const size_t sdesWords = (packet.size() - sdesStart) / sizeof(uint32_t);
    packet[sdesStart + 2] = static_cast<uint8_t>((sdesWords - 1U) >> 8U);
    packet[sdesStart + 3] = static_cast<uint8_t>(sdesWords - 1U);
    return packet;
}

TEST(RtcpPacketTest, ParsesServerSenderReportAndCname) {
    const auto report = parseRtcpSenderReport(makeServerCompoundPacket());

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->synchronizationSource, 0x0102'0304U);
    EXPECT_EQ(report->ntpTimestamp, 0x1122'3344'5566'7788ULL);
    EXPECT_EQ(report->rtpTimestamp, 0x99aa'bbccU);
    EXPECT_EQ(report->packetCount, 123U);
    EXPECT_EQ(report->octetCount, 45'678U);
    EXPECT_EQ(report->canonicalName, "creature-server@test");
}

TEST(RtcpPacketTest, SafelySkipsUnknownCompoundBlocks) {
    auto packet = makeServerCompoundPacket();
    const auto sdes = packet.begin() + 28;
    std::vector<uint8_t> withUnknown(packet.begin(), sdes);
    withUnknown.insert(withUnknown.end(), {0x80, 204, 0x00, 0x01, 0x12, 0x34, 0x56, 0x78});
    withUnknown.insert(withUnknown.end(), sdes, packet.end());

    const auto report = parseRtcpSenderReport(withUnknown);

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->synchronizationSource, 0x0102'0304U);
}

TEST(RtcpPacketTest, AcceptsAndSkipsSenderReportReceiverBlocks) {
    auto packet = makeServerCompoundPacket();
    packet[0] = 0x81;
    packet[2] = 0x00;
    packet[3] = 0x0c;
    packet.insert(packet.begin() + 28, 24, 0);

    const auto report = parseRtcpSenderReport(packet);

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->packetCount, 123U);
}

TEST(RtcpPacketTest, RejectsMalformedCompoundPackets) {
    auto truncated = makeServerCompoundPacket();
    truncated.pop_back();
    EXPECT_FALSE(parseRtcpSenderReport(truncated).has_value());

    auto invalidVersion = makeServerCompoundPacket();
    invalidVersion[0] = 0x40;
    EXPECT_FALSE(parseRtcpSenderReport(invalidVersion).has_value());

    auto invalidLength = makeServerCompoundPacket();
    invalidLength[3] = 0x7f;
    EXPECT_FALSE(parseRtcpSenderReport(invalidLength).has_value());

    auto unexpectedSenderReportExtension = makeServerCompoundPacket();
    unexpectedSenderReportExtension[3] = 0x07;
    unexpectedSenderReportExtension.insert(unexpectedSenderReportExtension.begin() + 28, 4, 0);
    EXPECT_FALSE(parseRtcpSenderReport(unexpectedSenderReportExtension).has_value());

    auto firstPacketIsNotSenderReport = makeServerCompoundPacket();
    firstPacketIsNotSenderReport[1] = 201;
    EXPECT_FALSE(parseRtcpSenderReport(firstPacketIsNotSenderReport).has_value());

    auto missingCname = makeServerCompoundPacket();
    missingCname[36] = 2;
    EXPECT_FALSE(parseRtcpSenderReport(missingCname).has_value());

    auto zeroSynchronizationSource = makeServerCompoundPacket(0);
    EXPECT_FALSE(parseRtcpSenderReport(zeroSynchronizationSource).has_value());

    std::vector<uint8_t> oversized(MAX_RTCP_PACKET_SIZE + sizeof(uint32_t), 0);
    EXPECT_FALSE(parseRtcpSenderReport(oversized).has_value());
}

TEST(RtcpPacketTest, RejectsMismatchedOrDuplicateCname) {
    auto mismatchedSource = makeServerCompoundPacket();
    mismatchedSource[32] ^= 0x01;
    EXPECT_FALSE(parseRtcpSenderReport(mismatchedSource).has_value());

    auto duplicateCname = makeServerCompoundPacket();
    const size_t endItem = duplicateCname.size() - 2;
    duplicateCname[endItem] = 1;
    duplicateCname[endItem + 1] = 0;
    EXPECT_FALSE(parseRtcpSenderReport(duplicateCname).has_value());
}

TEST(RtcpPacketTest, RejectsInvalidPaddingAndNonZeroSdesAlignment) {
    auto invalidPadding = makeServerCompoundPacket();
    invalidPadding[28] |= 0x20;
    invalidPadding.back() = 0xff;
    EXPECT_FALSE(parseRtcpSenderReport(invalidPadding).has_value());

    auto nonZeroAlignment = makeServerCompoundPacket(0x0102'0304U, "x");
    nonZeroAlignment.back() = 1;
    EXPECT_FALSE(parseRtcpSenderReport(nonZeroAlignment).has_value());
}

} // namespace
} // namespace creatures::audio

#include "audio/RtpJitterBuffer.h"
#include <gtest/gtest.h>
using namespace creatures::audio;
namespace {
ReceivedRtpPacket packet(uint32_t timestamp, uint32_t source = 1) {
    RtpPacket rtp;
    rtp.timestamp = timestamp;
    rtp.synchronizationSource = source;
    rtp.payload = {1, 2, 3};
    return {std::move(rtp), std::chrono::steady_clock::now()};
}
} // namespace
TEST(RtpJitterBufferTest, RetrievesReorderedFramesAndRejectsDuplicates) {
    RtpJitterBuffer buffer;
    buffer.push(packet(960));
    buffer.push(packet(480));
    EXPECT_EQ(buffer.push(packet(480)), RtpJitterBuffer::PushResult::Duplicate);
    EXPECT_EQ(buffer.initialPlayout()->timestamp, 480);
    ASSERT_TRUE(buffer.take(480));
    EXPECT_TRUE(buffer.peek(960));
    EXPECT_FALSE(buffer.take(480));
}
TEST(RtpJitterBufferTest, LatePacketsCannotReplayAfterReturningToIdle) {
    RtpJitterBuffer buffer;
    buffer.push(packet(480));
    buffer.discardBefore(960);
    EXPECT_EQ(buffer.push(packet(480)), RtpJitterBuffer::PushResult::Late);
    EXPECT_FALSE(buffer.initialPlayout());
}
TEST(RtpJitterBufferTest, NewSourceResetsTimelineAndDecoderGeneration) {
    RtpJitterBuffer buffer;
    buffer.push(packet(960));
    const auto generation = buffer.generation();
    buffer.discardBefore(1440);
    EXPECT_EQ(buffer.push(packet(0, 2)), RtpJitterBuffer::PushResult::NewSynchronizationSource);
    EXPECT_EQ(buffer.generation(), generation + 1);
    EXPECT_TRUE(buffer.take(0));
    EXPECT_FALSE(buffer.peek(960));
}
TEST(RtpJitterBufferTest, BufferIsBoundedAndTimestampDiscardHandlesWrap) {
    RtpJitterBuffer buffer(2);
    buffer.push(packet(0xfffffe20));
    buffer.push(packet(0));
    EXPECT_EQ(buffer.push(packet(480)), RtpJitterBuffer::PushResult::Overrun);
    EXPECT_EQ(buffer.size(), 2);
    buffer.discardBefore(480);
    EXPECT_FALSE(buffer.peek(0));
    EXPECT_TRUE(buffer.peek(480));
}

TEST(RtpJitterBufferTest, GenerationGuardCannotTakeNewSourceWithOldDecoderState) {
    RtpJitterBuffer buffer;
    buffer.push(packet(480));
    const auto old = buffer.generation();
    buffer.push(packet(480, 2));
    EXPECT_FALSE(buffer.take(480, old));
    EXPECT_FALSE(buffer.peek(480, old));
    buffer.discardBefore(960, old);
    EXPECT_TRUE(buffer.peek(480));
    EXPECT_TRUE(buffer.take(480, buffer.generation()));
}

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <gtest/gtest.h>

#include "audio/RtcpTiming.h"
#include "audio/audio-config.h"

namespace creatures::audio {
namespace {

using namespace std::chrono_literals;

constexpr int64_t NTP_UNIX_EPOCH_OFFSET_SECONDS = 2'208'988'800LL;

uint64_t makeNtp(int64_t unixSeconds, uint32_t fraction = 0) {
    return (static_cast<uint64_t>(unixSeconds + NTP_UNIX_EPOCH_OFFSET_SECONDS) << 32U) | fraction;
}

RtcpSenderReport makeReport(uint32_t synchronizationSource = 100, uint32_t rtpTimestamp = 10'000,
                            int64_t unixSeconds = 1'000) {
    return {
        .synchronizationSource = synchronizationSource,
        .ntpTimestamp = makeNtp(unixSeconds),
        .rtpTimestamp = rtpTimestamp,
        .packetCount = 12,
        .octetCount = 34,
        .canonicalName = "creature-server@test",
    };
}

TEST(RtcpReportCacheTest, CachesReportsBeforeRtpSourceIsKnown) {
    RtcpReportCache cache(2);
    const auto receivedAt = RtcpSteadyClock::time_point{} + 5s;

    EXPECT_FALSE(cache.store(makeReport(100), receivedAt));
    ASSERT_TRUE(cache.find(100).has_value());
    EXPECT_EQ(cache.find(100)->receivedAt, receivedAt);
    EXPECT_EQ(cache.find(100)->report.canonicalName, "creature-server@test");
}

TEST(RtcpReportCacheTest, UpdatesCurrentReportAndBoundsOldSources) {
    RtcpReportCache cache(2);
    EXPECT_FALSE(cache.store(makeReport(100), RtcpSteadyClock::time_point{} + 1s));

    auto updated = makeReport(100);
    updated.packetCount = 99;
    EXPECT_FALSE(cache.store(updated, RtcpSteadyClock::time_point{} + 2s));
    EXPECT_EQ(cache.size(), 1U);
    EXPECT_EQ(cache.find(100)->report.packetCount, 99U);

    EXPECT_FALSE(cache.store(makeReport(101), RtcpSteadyClock::time_point{} + 3s));
    EXPECT_TRUE(cache.store(makeReport(102), RtcpSteadyClock::time_point{} + 4s));
    EXPECT_FALSE(cache.find(100).has_value());
    EXPECT_TRUE(cache.find(101).has_value());
    EXPECT_TRUE(cache.find(102).has_value());
}

TEST(RtcpReportCacheTest, RejectsZeroCapacity) { EXPECT_THROW(RtcpReportCache(0), std::invalid_argument); }

TEST(RtcpTimingTest, ConvertsNtpSecondsAndFractionToSystemTime) {
    const auto unixEpoch = ntpToSystemTime(makeNtp(0));
    const auto halfSecond = ntpToSystemTime(makeNtp(0, 0x8000'0000U));

    ASSERT_TRUE(unixEpoch.has_value());
    ASSERT_TRUE(halfSecond.has_value());
    EXPECT_EQ(*unixEpoch, RtcpSystemClock::time_point{});
    EXPECT_EQ(*halfSecond, RtcpSystemClock::time_point{} + 500ms);
}

TEST(RtcpTimingTest, UsesSignedWrapSafeRtpDifferences) {
    EXPECT_EQ(signedRtpTimestampDifference(20U, 10U), 10);
    EXPECT_EQ(signedRtpTimestampDifference(10U, 20U), -10);
    EXPECT_EQ(signedRtpTimestampDifference(5U, std::numeric_limits<uint32_t>::max() - 4U), 10);
    EXPECT_EQ(signedRtpTimestampDifference(std::numeric_limits<uint32_t>::max() - 4U, 5U), -10);
    EXPECT_EQ(signedRtpTimestampDifference(0x8000'0000U, 0U), std::numeric_limits<int32_t>::min());
}

TEST(RtcpTimingTest, MapsRtpSamplesAcrossRollover) {
    auto report = makeReport(100, std::numeric_limits<uint32_t>::max() - 239U);
    const auto mediaTime = rtpSystemTime(report, 240U);

    ASSERT_TRUE(mediaTime.has_value());
    EXPECT_EQ(*mediaTime, RtcpSystemClock::time_point{} + 1'000s + 10ms);
}

TEST(RtcpTimingTest, ClockPairConversionDoesNotReadWallClockAgain) {
    const RtcpClockPair clockPair{
        .systemTime = RtcpSystemClock::time_point{} + 1'000s,
        .steadyTime = RtcpSteadyClock::time_point{} + 20s,
    };

    EXPECT_EQ(clockPair.toSteady(RtcpSystemClock::time_point{} + 1'003s), RtcpSteadyClock::time_point{} + 23s);
}

TEST(RtcpTimingTest, ReservesIdleEnqueueHeadroomForTwentyMillisecondPlayout) {
    EXPECT_EQ(rtcpIdleQueueFrames(20, 0), 15U * SAMPLE_RATE / 1000U);
    EXPECT_EQ(rtcpIdleQueueFrames(20, 3), 12U * SAMPLE_RATE / 1000U);
    EXPECT_EQ(rtcpIdleQueueFrames(20, -3), 15U * SAMPLE_RATE / 1000U);
    EXPECT_EQ(rtcpIdleQueueFrames(50, 0), TARGET_PLAYOUT_FRAMES * FRAMES_PER_CHUNK);
}

TEST(RtcpTimingTest, IdenticalMappingProducesIdenticalDeadlineRegardlessOfArrival) {
    const RtcpClockPair clockPair{
        .systemTime = RtcpSystemClock::time_point{} + 1'000s,
        .steadyTime = RtcpSteadyClock::time_point{} + 20s,
    };
    const RtcpPlayoutPlanner firstPlanner(clockPair, 20ms, 0ms);
    const RtcpPlayoutPlanner secondPlanner(clockPair, 20ms, 0ms);
    const auto report = makeReport();

    const auto first = firstPlanner.plan(report, report.rtpTimestamp, FRAMES_PER_CHUNK * 2);
    const auto second = secondPlanner.plan(report, report.rtpTimestamp, FRAMES_PER_CHUNK * 2);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->presentationDeadline, RtcpSteadyClock::time_point{} + 20s + 20ms);
    EXPECT_EQ(first->enqueueDeadline, RtcpSteadyClock::time_point{} + 20s);
    EXPECT_EQ(first->presentationDeadline, second->presentationDeadline);
    EXPECT_EQ(first->enqueueDeadline, second->enqueueDeadline);
}

TEST(RtcpTimingTest, PositiveDeviceCompensationSchedulesEarlier) {
    const RtcpClockPair clockPair{
        .systemTime = RtcpSystemClock::time_point{} + 1'000s,
        .steadyTime = RtcpSteadyClock::time_point{} + 20s,
    };
    const RtcpPlayoutPlanner planner(clockPair, 20ms, 3ms);
    const auto report = makeReport();

    const auto plan = planner.plan(report, report.rtpTimestamp, FRAMES_PER_CHUNK * 2);

    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->presentationDeadline, RtcpSteadyClock::time_point{} + 20s + 20ms);
    EXPECT_EQ(plan->enqueueDeadline, RtcpSteadyClock::time_point{} + 20s - 3ms);
}

TEST(RtcpTimingTest, SelectsTheFirstFutureTimestampAfterALateJoin) {
    const RtcpClockPair clockPair{
        .systemTime = RtcpSystemClock::time_point{} + 1'000s,
        .steadyTime = RtcpSteadyClock::time_point{} + 20s,
    };
    const RtcpPlayoutPlanner planner(clockPair, 20ms, 0ms);
    const auto report = makeReport();
    const auto now = RtcpSteadyClock::time_point{} + 20s + 3ms;

    const auto current = planner.plan(report, report.rtpTimestamp, FRAMES_PER_CHUNK * 2);
    const auto next = planner.plan(report, report.rtpTimestamp + FRAMES_PER_CHUNK, FRAMES_PER_CHUNK * 2);

    ASSERT_TRUE(current.has_value());
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(classifyRtcpEnqueue(*current, now, 2ms), RtcpEnqueueState::Missed);
    EXPECT_EQ(classifyRtcpEnqueue(*next, now, 2ms), RtcpEnqueueState::Wait);
}

TEST(RtcpTimingTest, EnforcesConfiguredClockSynchronizationGuard) {
    const RtcpPlayoutPlan plan{
        .mediaSystemTime = RtcpSystemClock::time_point{},
        .presentationDeadline = RtcpSteadyClock::time_point{} + 500ms,
        .enqueueDeadline = RtcpSteadyClock::time_point{} + 480ms,
    };
    const auto maximumDifference = std::chrono::milliseconds(RTCP_MAX_DEADLINE_DISTANCE_MS);

    EXPECT_TRUE(
        rtcpPresentationDeadlinePlausible(plan, RtcpSteadyClock::time_point{} + 470ms, 20ms, maximumDifference));
    EXPECT_TRUE(
        rtcpPresentationDeadlinePlausible(plan, RtcpSteadyClock::time_point{} + 490ms, 20ms, maximumDifference));
    EXPECT_FALSE(
        rtcpPresentationDeadlinePlausible(plan, RtcpSteadyClock::time_point{} + 469ms, 20ms, maximumDifference));
    EXPECT_FALSE(
        rtcpPresentationDeadlinePlausible(plan, RtcpSteadyClock::time_point{} + 491ms, 20ms, maximumDifference));
}

TEST(RtcpTimingTest, VerifiesCnameAndClockRelationship) {
    auto first = makeReport(100, 10'000, 1'000);
    auto second = makeReport(101, 58'000, 1'001);

    EXPECT_TRUE(rtcpClockMappingsCompatible(first, second, 1us));

    second.canonicalName = "other-server";
    EXPECT_FALSE(rtcpClockMappingsCompatible(first, second, 1ms));

    second.canonicalName = first.canonicalName;
    second.ntpTimestamp = makeNtp(1'001, 0x0100'0000U);
    EXPECT_FALSE(rtcpClockMappingsCompatible(first, second, 1ms));
}

} // namespace
} // namespace creatures::audio

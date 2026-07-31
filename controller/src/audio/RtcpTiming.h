#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "audio/RtcpPacket.h"

namespace creatures::audio {

using RtcpSteadyClock = std::chrono::steady_clock;
using RtcpSystemClock = std::chrono::system_clock;

struct TimedRtcpSenderReport {
    RtcpSenderReport report;
    RtcpSteadyClock::time_point receivedAt;
};

class RtcpReportCache {
  public:
    explicit RtcpReportCache(size_t capacity);

    bool store(RtcpSenderReport report, RtcpSteadyClock::time_point receivedAt);
    [[nodiscard]] std::optional<TimedRtcpSenderReport> find(uint32_t synchronizationSource) const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_t size() const;

  private:
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, TimedRtcpSenderReport> reports_;
    std::deque<uint32_t> insertionOrder_;
};

struct RtcpClockPair {
    RtcpSystemClock::time_point systemTime;
    RtcpSteadyClock::time_point steadyTime;

    [[nodiscard]] static RtcpClockPair capture();
    [[nodiscard]] RtcpSteadyClock::time_point toSteady(RtcpSystemClock::time_point time) const;
};

struct RtcpPlayoutPlan {
    RtcpSystemClock::time_point mediaSystemTime;
    RtcpSteadyClock::time_point presentationDeadline;
    RtcpSteadyClock::time_point enqueueDeadline;
};

enum class RtcpEnqueueState {
    Wait,
    Ready,
    Missed,
};

class RtcpPlayoutPlanner {
  public:
    RtcpPlayoutPlanner(RtcpClockPair clockPair, std::chrono::milliseconds commonPlayoutDelay,
                       std::chrono::milliseconds deviceCompensation);

    [[nodiscard]] std::optional<RtcpPlayoutPlan> plan(const RtcpSenderReport &report, uint32_t rtpTimestamp,
                                                      size_t queuedFrames) const;

  private:
    RtcpClockPair clockPair_;
    std::chrono::milliseconds commonPlayoutDelay_;
    std::chrono::milliseconds deviceCompensation_;
};

[[nodiscard]] size_t rtcpIdleQueueFrames(uint16_t commonPlayoutDelayMs, int16_t deviceCompensationMs);
[[nodiscard]] std::optional<RtcpSystemClock::time_point> ntpToSystemTime(uint64_t ntpTimestamp);
[[nodiscard]] int32_t signedRtpTimestampDifference(uint32_t timestamp, uint32_t reference);
[[nodiscard]] std::optional<RtcpSystemClock::time_point> rtpSystemTime(const RtcpSenderReport &report,
                                                                       uint32_t rtpTimestamp);
[[nodiscard]] bool rtcpClockMappingsCompatible(const RtcpSenderReport &first, const RtcpSenderReport &second,
                                               std::chrono::microseconds tolerance);
[[nodiscard]] bool rtcpPresentationDeadlinePlausible(const RtcpPlayoutPlan &plan,
                                                     RtcpSteadyClock::time_point packetArrival,
                                                     std::chrono::milliseconds commonPlayoutDelay,
                                                     std::chrono::milliseconds maximumDifference);
[[nodiscard]] RtcpEnqueueState classifyRtcpEnqueue(const RtcpPlayoutPlan &plan, RtcpSteadyClock::time_point now,
                                                   std::chrono::microseconds lateTolerance);

} // namespace creatures::audio

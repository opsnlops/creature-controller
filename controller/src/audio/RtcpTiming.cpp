#include "audio/RtcpTiming.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include "audio/audio-config.h"

namespace creatures::audio {
namespace {

constexpr int64_t NTP_UNIX_EPOCH_OFFSET_SECONDS = 2'208'988'800LL;
constexpr uint64_t NTP_FRACTION_SCALE = uint64_t{1} << 32U;
constexpr int64_t NANOSECONDS_PER_SECOND = 1'000'000'000LL;

std::chrono::nanoseconds samplesToDuration(int32_t samples) {
    const auto nanoseconds = static_cast<int64_t>(samples) * NANOSECONDS_PER_SECOND / static_cast<int64_t>(SAMPLE_RATE);
    return std::chrono::nanoseconds(nanoseconds);
}

std::chrono::nanoseconds framesToDuration(size_t frames) {
    const uint64_t wholeSeconds = frames / SAMPLE_RATE;
    const uint64_t remainingFrames = frames % SAMPLE_RATE;
    const uint64_t nanoseconds = wholeSeconds * static_cast<uint64_t>(NANOSECONDS_PER_SECOND) +
                                 remainingFrames * static_cast<uint64_t>(NANOSECONDS_PER_SECOND) / SAMPLE_RATE;
    return std::chrono::nanoseconds(
        static_cast<int64_t>(std::min<uint64_t>(nanoseconds, std::numeric_limits<int64_t>::max())));
}

} // namespace

RtcpReportCache::RtcpReportCache(size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) {
        throw std::invalid_argument("RTCP report cache capacity must be greater than zero");
    }
}

bool RtcpReportCache::store(RtcpSenderReport report, RtcpSteadyClock::time_point receivedAt) {
    std::lock_guard<std::mutex> lock(mutex_);
    const uint32_t synchronizationSource = report.synchronizationSource;
    auto existing = reports_.find(synchronizationSource);
    if (existing != reports_.end()) {
        existing->second = TimedRtcpSenderReport{std::move(report), receivedAt};
        return false;
    }

    bool evicted = false;
    if (reports_.size() >= capacity_) {
        reports_.erase(insertionOrder_.front());
        insertionOrder_.pop_front();
        evicted = true;
    }
    insertionOrder_.push_back(synchronizationSource);
    reports_.emplace(synchronizationSource, TimedRtcpSenderReport{std::move(report), receivedAt});
    return evicted;
}

std::optional<TimedRtcpSenderReport> RtcpReportCache::find(uint32_t synchronizationSource) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto report = reports_.find(synchronizationSource);
    if (report == reports_.end()) {
        return std::nullopt;
    }
    return report->second;
}

bool RtcpReportCache::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return reports_.empty();
}

size_t RtcpReportCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return reports_.size();
}

RtcpClockPair RtcpClockPair::capture() {
    const auto steadyBefore = RtcpSteadyClock::now();
    const auto systemTime = RtcpSystemClock::now();
    const auto steadyAfter = RtcpSteadyClock::now();
    return {systemTime, steadyBefore + (steadyAfter - steadyBefore) / 2};
}

RtcpSteadyClock::time_point RtcpClockPair::toSteady(RtcpSystemClock::time_point time) const {
    return steadyTime + std::chrono::duration_cast<RtcpSteadyClock::duration>(time - systemTime);
}

size_t rtcpIdleQueueFrames(uint16_t commonPlayoutDelayMs, int16_t deviceCompensationMs) {
    const int positiveCompensation = std::max<int>(0, deviceCompensationMs);
    const int availableQueueMilliseconds =
        static_cast<int>(commonPlayoutDelayMs) - RTCP_ENQUEUE_HEADROOM_MS - positiveCompensation;
    const int queueMilliseconds = std::max<int>(FRAME_MS, availableQueueMilliseconds);
    const size_t configuredFrames = static_cast<size_t>(queueMilliseconds) * SAMPLE_RATE / 1000U;
    return std::min(TARGET_PLAYOUT_FRAMES * static_cast<size_t>(FRAMES_PER_CHUNK), configuredFrames);
}

RtcpPlayoutPlanner::RtcpPlayoutPlanner(RtcpClockPair clockPair, std::chrono::milliseconds commonPlayoutDelay,
                                       std::chrono::milliseconds deviceCompensation)
    : clockPair_(clockPair), commonPlayoutDelay_(commonPlayoutDelay), deviceCompensation_(deviceCompensation) {}

std::optional<RtcpPlayoutPlan> RtcpPlayoutPlanner::plan(const RtcpSenderReport &report, uint32_t rtpTimestamp,
                                                        size_t queuedFrames) const {
    const auto mediaTime = rtpSystemTime(report, rtpTimestamp);
    if (!mediaTime.has_value()) {
        return std::nullopt;
    }

    const auto presentationSystemTime = *mediaTime + commonPlayoutDelay_;
    const auto presentationDeadline = clockPair_.toSteady(presentationSystemTime);
    const auto enqueueDeadline = presentationDeadline - framesToDuration(queuedFrames) - deviceCompensation_;
    return RtcpPlayoutPlan{
        .mediaSystemTime = *mediaTime,
        .presentationDeadline = presentationDeadline,
        .enqueueDeadline = enqueueDeadline,
    };
}

std::optional<RtcpSystemClock::time_point> ntpToSystemTime(uint64_t ntpTimestamp) {
    const int64_t ntpSeconds = static_cast<int64_t>(ntpTimestamp >> 32U);
    const int64_t unixSeconds = ntpSeconds - NTP_UNIX_EPOCH_OFFSET_SECONDS;
    const uint64_t fraction = static_cast<uint32_t>(ntpTimestamp);
    const uint64_t fractionNanoseconds = fraction * static_cast<uint64_t>(NANOSECONDS_PER_SECOND) / NTP_FRACTION_SCALE;

    const auto duration = std::chrono::seconds(unixSeconds) + std::chrono::nanoseconds(fractionNanoseconds);
    const auto minimum = RtcpSystemClock::time_point::min().time_since_epoch();
    const auto maximum = RtcpSystemClock::time_point::max().time_since_epoch();
    const auto converted = std::chrono::duration_cast<RtcpSystemClock::duration>(duration);
    if (converted < minimum || converted > maximum) {
        return std::nullopt;
    }
    return RtcpSystemClock::time_point(converted);
}

int32_t signedRtpTimestampDifference(uint32_t timestamp, uint32_t reference) {
    const uint32_t difference = timestamp - reference;
    if (difference <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        return static_cast<int32_t>(difference);
    }
    return static_cast<int32_t>(static_cast<int64_t>(difference) - (int64_t{1} << 32U));
}

std::optional<RtcpSystemClock::time_point> rtpSystemTime(const RtcpSenderReport &report, uint32_t rtpTimestamp) {
    const auto reportTime = ntpToSystemTime(report.ntpTimestamp);
    if (!reportTime.has_value()) {
        return std::nullopt;
    }
    const auto mediaTime = reportTime->time_since_epoch() +
                           std::chrono::duration_cast<RtcpSystemClock::duration>(
                               samplesToDuration(signedRtpTimestampDifference(rtpTimestamp, report.rtpTimestamp)));
    return RtcpSystemClock::time_point(mediaTime);
}

bool rtcpClockMappingsCompatible(const RtcpSenderReport &first, const RtcpSenderReport &second,
                                 std::chrono::microseconds tolerance) {
    if (first.canonicalName.empty() || first.canonicalName != second.canonicalName) {
        return false;
    }

    const auto firstTime = rtpSystemTime(first, second.rtpTimestamp);
    const auto secondTime = ntpToSystemTime(second.ntpTimestamp);
    if (!firstTime.has_value() || !secondTime.has_value()) {
        return false;
    }

    const auto difference = *firstTime >= *secondTime ? *firstTime - *secondTime : *secondTime - *firstTime;
    return difference <= tolerance;
}

bool rtcpPresentationDeadlinePlausible(const RtcpPlayoutPlan &plan, RtcpSteadyClock::time_point packetArrival,
                                       std::chrono::milliseconds commonPlayoutDelay,
                                       std::chrono::milliseconds maximumDifference) {
    const auto expectedDeadline = packetArrival + commonPlayoutDelay;
    const auto difference = plan.presentationDeadline >= expectedDeadline
                                ? plan.presentationDeadline - expectedDeadline
                                : expectedDeadline - plan.presentationDeadline;
    return difference <= maximumDifference;
}

RtcpEnqueueState classifyRtcpEnqueue(const RtcpPlayoutPlan &plan, RtcpSteadyClock::time_point now,
                                     std::chrono::microseconds lateTolerance) {
    if (now < plan.enqueueDeadline) {
        return RtcpEnqueueState::Wait;
    }
    if (now - plan.enqueueDeadline > lateTolerance) {
        return RtcpEnqueueState::Missed;
    }
    return RtcpEnqueueState::Ready;
}

} // namespace creatures::audio

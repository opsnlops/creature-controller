#pragma once
#include "audio/RtpPacket.h"
#include "audio/audio-config.h"
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
namespace creatures::audio {
struct ReceivedRtpPacket {
    RtpPacket packet;
    std::chrono::steady_clock::time_point arrival;
};
class RtpJitterBuffer {
  public:
    explicit RtpJitterBuffer(size_t capacity = RTP_JITTER_BUFFER_FRAMES) : capacity_(capacity) {
        if (!capacity)
            throw std::invalid_argument("Jitter buffer needs a positive capacity");
    }
    enum class PushResult {
        Late,
        Accepted,
        Duplicate,
        Overrun,
        NewSynchronizationSource,
    };

    struct InitialPlayout {
        uint32_t timestamp;
        std::chrono::steady_clock::time_point startTime;
        uint64_t generation;
        uint32_t synchronizationSource;
        std::chrono::steady_clock::time_point arrival;
    };

    void clear() {
        std::lock_guard lock(mutex_);
        frames_.clear();
        insertionOrder_.clear();
        synchronizationSource_.reset();
        lastArrival_.reset();
        minimumTimestamp_.reset();
        generation_ = 0;
    }

    PushResult push(ReceivedRtpPacket packet) {
        std::lock_guard<std::mutex> lock(mutex_);

        bool synchronizationSourceChanged = false;
        if (!synchronizationSource_.has_value() || *synchronizationSource_ != packet.packet.synchronizationSource) {
            frames_.clear();
            insertionOrder_.clear();
            synchronizationSource_ = packet.packet.synchronizationSource;
            minimumTimestamp_.reset();
            ++generation_;
            synchronizationSourceChanged = true;
        }
        if (minimumTimestamp_ && static_cast<int32_t>(packet.packet.timestamp - *minimumTimestamp_) < 0)
            return PushResult::Late;
        lastArrival_ = packet.arrival;

        if (frames_.contains(packet.packet.timestamp)) {
            return PushResult::Duplicate;
        }

        bool overrun = false;
        while (frames_.size() >= capacity_) {
            discardOldestLocked();
            overrun = true;
        }

        const uint32_t timestamp = packet.packet.timestamp;
        frames_.emplace(timestamp, std::make_shared<const ReceivedRtpPacket>(std::move(packet)));
        insertionOrder_.push_back(timestamp);

        if (synchronizationSourceChanged) {
            return PushResult::NewSynchronizationSource;
        }
        return overrun ? PushResult::Overrun : PushResult::Accepted;
    }

    std::shared_ptr<const ReceivedRtpPacket> take(uint32_t timestamp,
                                                  std::optional<uint64_t> generation = std::nullopt) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (generation && *generation != generation_)
            return {};
        auto frame = frames_.find(timestamp);
        if (frame == frames_.end()) {
            return {};
        }

        auto packet = std::move(frame->second);
        frames_.erase(frame);
        return packet;
    }

    std::shared_ptr<const ReceivedRtpPacket> peek(uint32_t timestamp,
                                                  std::optional<uint64_t> generation = std::nullopt) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (generation && *generation != generation_)
            return {};
        const auto frame = frames_.find(timestamp);
        if (frame == frames_.end()) {
            return {};
        }
        return frame->second;
    }

    bool contains(uint32_t timestamp) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_.contains(timestamp);
    }

    std::optional<InitialPlayout> initialPlayout() {
        std::lock_guard<std::mutex> lock(mutex_);
        discardConsumedOrderEntriesLocked();
        if (insertionOrder_.empty()) {
            return std::nullopt;
        }

        uint32_t timestamp = insertionOrder_.front();
        for (const auto &[candidate, packet] : frames_) {
            (void)packet;
            if (static_cast<int32_t>(candidate - timestamp) < 0)
                timestamp = candidate;
        }
        const auto frame = frames_.find(timestamp);
        if (frame == frames_.end()) {
            return std::nullopt;
        }

        return InitialPlayout{
            .timestamp = timestamp,
            .startTime = frame->second->arrival + std::chrono::milliseconds(FRAME_MS),
            .generation = generation_,
            .synchronizationSource = *synchronizationSource_,
            .arrival = frame->second->arrival,
        };
    }

    void discardBefore(uint32_t timestamp, std::optional<uint64_t> generation = std::nullopt) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (generation && *generation != generation_)
            return;
        minimumTimestamp_ = timestamp;
        for (auto frame = frames_.begin(); frame != frames_.end();) {
            if (static_cast<int32_t>(frame->first - timestamp) < 0) {
                frame = frames_.erase(frame);
            } else {
                ++frame;
            }
        }
        discardConsumedOrderEntriesLocked();
    }

    [[nodiscard]] uint64_t generation() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return generation_;
    }

    [[nodiscard]] std::optional<uint32_t> synchronizationSource() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return synchronizationSource_;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_.size();
    }

    [[nodiscard]] bool isIdleFor(std::chrono::milliseconds duration) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastArrival_.has_value() && std::chrono::steady_clock::now() - *lastArrival_ >= duration;
    }

  private:
    void discardOldestLocked() {
        discardConsumedOrderEntriesLocked();
        if (insertionOrder_.empty()) {
            frames_.clear();
            return;
        }
        frames_.erase(insertionOrder_.front());
        insertionOrder_.pop_front();
    }

    void discardConsumedOrderEntriesLocked() {
        while (!insertionOrder_.empty() && !frames_.contains(insertionOrder_.front())) {
            insertionOrder_.pop_front();
        }
    }

    const size_t capacity_;
    std::optional<uint32_t> minimumTimestamp_;
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, std::shared_ptr<const ReceivedRtpPacket>> frames_;
    std::deque<uint32_t> insertionOrder_;
    std::optional<uint32_t> synchronizationSource_;
    std::optional<std::chrono::steady_clock::time_point> lastArrival_;
    uint64_t generation_{0};
};

} // namespace creatures::audio

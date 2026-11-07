#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>

namespace creatures {

/**
 * A simple thread-safe message queue
 *
 * Used for passing messages around between threads in order
 *
 * @tparam T
 */
template <typename T> class MessageQueue {
  public:
    MessageQueue() : shutdown_requested(false) {}

    /**
     * Create a queue that sheds old messages instead of growing without limit
     *
     * When the queue is full, push() discards the oldest message that canDrop
     * accepts. Messages canDrop rejects are never discarded, so the queue can
     * exceed maxSize if it fills with nothing but those.
     *
     * @param maxSize the number of messages to hold before shedding
     * @param canDrop decides if a queued message may be discarded
     */
    MessageQueue(std::size_t maxSize, std::function<bool(const T &)> canDrop)
        : shutdown_requested(false), maxSize(maxSize), canDrop(std::move(canDrop)) {}

    void push(T message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (shutdown_requested.load()) {
            return; // Don't accept new messages during shutdown
        }
        if (maxSize > 0 && queue.size() >= maxSize) {
            auto oldest = std::find_if(queue.begin(), queue.end(), canDrop);
            if (oldest != queue.end()) {
                queue.erase(oldest);
                dropped.fetch_add(1);
            }
        }
        queue.push_back(std::move(message));
        cond.notify_one(); // Hop, hop! A new message is here!
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cond.wait(lock, [this] {
            return !queue.empty() || shutdown_requested.load();
        }); // Wait patiently like a bunny in a burrow

        if (shutdown_requested.load() && queue.empty()) {
            // Return a default-constructed message during shutdown
            return T{};
        }

        T msg = std::move(queue.front());
        queue.pop_front();
        return msg;
    }

    /**
     * Pop a message with a timeout - perfect for when you don't want to wait
     * forever like a rabbit watching carrots grow!
     *
     * @param timeout how long to wait for a message
     * @return optional containing the message if one arrived in time, nullopt
     * if we timed out
     */
    std::optional<T> pop_timeout(const std::chrono::milliseconds &timeout) {
        std::unique_lock<std::mutex> lock(mtx);
        if (cond.wait_for(lock, timeout, [this] { return !queue.empty(); })) {
            T msg = std::move(queue.front());
            queue.pop_front();
            return msg;
        }
        return std::nullopt; // Timeout - no carrots today!
    }

    /**
     * Clear all messages from the queue - like cleaning out a rabbit hutch!
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        queue.clear();
    }

    /**
     * Request shutdown - wakes up any threads waiting on pop()
     */
    void request_shutdown() {
        std::lock_guard<std::mutex> lock(mtx);
        shutdown_requested.store(true);
        cond.notify_all(); // Wake up all waiting threads
    }

    /**
     * Check if shutdown has been requested
     */
    bool is_shutdown_requested() const { return shutdown_requested.load(); }

    /**
     * Check if the queue is empty - useful for peeking into the rabbit hole
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

    /**
     * Get the size of the queue - count how many carrots are waiting!
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }

    /**
     * The total number of messages discarded because the queue was full
     */
    std::uint64_t droppedCount() const { return dropped.load(); }

  private:
    mutable std::mutex mtx; // Made mutable so const methods can use it
    std::condition_variable cond;
    std::deque<T> queue;
    std::atomic<bool> shutdown_requested;
    std::size_t maxSize = 0; // Zero means unbounded
    std::function<bool(const T &)> canDrop;
    std::atomic<std::uint64_t> dropped{0};
};
} // namespace creatures
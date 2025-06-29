#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <chrono>
#include <optional>

namespace creatures {

    /**
     * A simple thread-safe message queue
     *
     * Used for passing messages around between threads in order
     *
     * @tparam T
     */
    template<typename T>
    class MessageQueue {
    public:
        void push(T message) {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push_back(std::move(message));
            cond.notify_one(); // Hop, hop! A new message is here!
        }

        T pop() {
            std::unique_lock<std::mutex> lock(mtx);
            cond.wait(lock, [this] { return !queue.empty(); }); // Wait patiently like a bunny in a burrow
            T msg = std::move(queue.front());
            queue.pop_front();
            return msg;
        }

        /**
         * Pop a message with a timeout - perfect for when you don't want to wait forever like a rabbit watching carrots grow!
         *
         * @param timeout how long to wait for a message
         * @return optional containing the message if one arrived in time, nullopt if we timed out
         */
        std::optional<T> pop_timeout(const std::chrono::milliseconds& timeout) {
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

    private:
        mutable std::mutex mtx;  // Made mutable so const methods can use it
        std::condition_variable cond;
        std::deque<T> queue;
    };
}
#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <chrono>

namespace creatures {

    /**
     * A simple thread-safe message queue
     *
     * Used for passing messages around between threads in order
     * Now with timeout support so threads can check for shutdown requests!
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

        // New method: pop with timeout
        std::optional<T> pop_timeout(std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mtx);
            if (cond.wait_for(lock, timeout, [this] { return !queue.empty(); })) {
                T msg = std::move(queue.front());
                queue.pop_front();
                return msg;
            }
            return std::nullopt; // Timeout - no message available
        }

        // Check if queue is empty (useful for non-blocking checks)
        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.empty();
        }

        // Get the current size of the queue
        size_t size() const {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.size();
        }

    private:
        mutable std::mutex mtx;
        std::condition_variable cond;
        std::deque<T> queue;
    };
}
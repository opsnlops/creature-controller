#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <chrono>
#include <stdexcept>

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
         * Pop a message with a timeout
         *
         * @param timeout Maximum time to wait for a message
         * @return The message if available
         * @throws std::runtime_error if timeout is reached
         */
        T popWithTimeout(std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mtx);
            bool hasMessage = cond.wait_for(lock, timeout, [this] { return !queue.empty(); });

            if (!hasMessage) {
                throw std::runtime_error("Timeout waiting for message");
            }

            T msg = std::move(queue.front());
            queue.pop_front();
            return msg;
        }

        /**
         * Check if the queue is empty
         *
         * @return true if the queue is empty
         */
        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.empty();
        }

        /**
         * Get the number of messages in the queue
         *
         * @return The queue size
         */
        size_t size() const {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.size();
        }

        /**
         * Clear all messages from the queue
         */
        void clear() {
            std::lock_guard<std::mutex> lock(mtx);
            queue.clear();
        }

    private:
        mutable std::mutex mtx;
        std::condition_variable cond;
        std::deque<T> queue;
    };
}
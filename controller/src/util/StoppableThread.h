#pragma once

#include <atomic>
#include <thread>
#include <memory>

namespace creatures {

    /**
     * A simple thread class that can be stopped
     */
    class StoppableThread {
    public:
        StoppableThread() : stop_requested(false), thread_started(false), thread_joined(false) {}

        virtual ~StoppableThread() {
            // Signal thread to stop
            requestStop();

            // If thread is joinable but we can't safely join it,
            // we must detach to prevent std::terminate on destruction
            if (thread_started && !thread_joined && thread.joinable()) {
                // Get current thread ID to avoid joining ourselves
                std::thread::id currentId = std::this_thread::get_id();

                if (thread.get_id() != currentId) {
                    // Safe to join
                    thread.join();
                    thread_joined = true;
                } else {
                    // Not safe to join (would deadlock), must detach
                    thread.detach();
                }
            }
        }

        std::string getName() {
            return threadName;
        }

        bool isThreadJoinable() {
            return thread_started && !thread_joined && thread.joinable();
        }

        virtual void start() {
            if (thread_started) {
                return; // Don't start if already running
            }

            thread_started = true;
            thread_joined = false;
            thread = std::thread(&StoppableThread::threadFunction, this);
        }

        // Virtual shutdown that derived classes can override
        virtual void shutdown() {
            // First request the thread to stop
            requestStop();

            // Try to safely join the thread if it's not the current thread
            tryJoin();
        }

        // Request the thread to stop without joining
        void requestStop() {
            stop_requested.store(true);
        }

        // Check if thread is running
        bool isRunning() const {
            return thread_started && !thread_joined;
        }

        // Try to join the thread safely - will not join if it's the current thread
        bool tryJoin() {
            // Get current thread ID to avoid joining ourselves
            std::thread::id currentId = std::this_thread::get_id();

            // Only try to join if:
            // 1. Thread is started
            // 2. Hasn't been joined yet
            // 3. Is joinable
            // 4. We're not trying to join ourselves (would cause deadlock)
            if (thread_started && !thread_joined && thread.joinable() && thread.get_id() != currentId) {
                thread.join();
                thread_joined = true;
                return true;
            }
            return false;
        }

    protected:
        // The derived class implements this
        virtual void run() = 0;

        std::atomic<bool> stop_requested;
        std::string threadName = "unnamed thread";

    private:
        // Non-virtual wrapper for the run method
        void threadFunction() {
            run();  // Call the derived class's implementation
        }

        std::thread thread;
        std::atomic<bool> thread_started;
        std::atomic<bool> thread_joined;
    };

} // namespace creatures
//
// StoppableThread.h
//

#pragma once

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace creatures {

/**
 * A simple thread class that can be stopped
 */
class StoppableThread {
  public:
    StoppableThread() : stop_requested(false) {}

    virtual ~StoppableThread() {
        // More aggressive shutdown in destructor
        if (stop_requested.load()) {
            // Thread was already asked to stop, give it a bit more time
            joinWithTimeout(std::chrono::milliseconds(100));
        } else {
            // First time - request stop and wait a bit longer
            stop_requested.store(true);
            joinWithTimeout(std::chrono::milliseconds(500));
        }
    }

    std::string getName() { return threadName; }

    bool isThreadJoinable() { return thread.joinable(); }

    virtual void start() { thread = std::thread(&StoppableThread::run, this); }

    virtual void shutdown() {
        stop_requested.store(true);

        // Give the thread a reasonable amount of time to finish
        joinWithTimeout(std::chrono::milliseconds(200));
    }

  protected:
    virtual void
    run() = 0; // This is the method that will be called when the thread starts

    std::atomic<bool> stop_requested;
    std::string threadName = "unnamed thread";

  private:
    std::thread thread;

    /**
     * Join the thread with a timeout - if it doesn't join in time, detach it
     * This prevents hanging forever on stuck threads
     */
    void joinWithTimeout(std::chrono::milliseconds timeout) {
        if (!thread.joinable()) {
            return;
        }

        // Try to join with timeout
        auto future = std::async(std::launch::async, [this]() {
            if (thread.joinable()) {
                thread.join();
            }
        });

        if (future.wait_for(timeout) == std::future_status::timeout) {
            // Thread didn't finish in time - detach it so we don't block
            // This isn't ideal but prevents hanging
            if (thread.joinable()) {
                thread.detach();
            }
        }
    }
};

} // namespace creatures
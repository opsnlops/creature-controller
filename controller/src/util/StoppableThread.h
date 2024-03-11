
#pragma once

#include <atomic>
#include <thread>

namespace creatures {

    class StoppableThread {
    public:
        StoppableThread() : stop_requested(false) {}
        virtual ~StoppableThread() {
            if (thread.joinable()) {
                thread.join();
            }
        }

        virtual void start() {
            thread = std::thread(&StoppableThread::run, this);
        }

        void shutdown() {
            stop_requested.store(true);
        }

    protected:
        virtual void run() = 0; // Work to be done by the thread

        std::atomic<bool> stop_requested;

    private:
        std::thread thread;
    };

} // namespace creatures


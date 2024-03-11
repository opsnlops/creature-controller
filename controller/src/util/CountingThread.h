
#pragma once

#include "util/StoppableThread.h"
#include <chrono>

/**
 * @brief A simple thread that counts up until it is stopped
 *
 * This may seem silly (because it is), but it's used to test the functionality of StoppableThread.
 *
 */

namespace creatures {

    class CountingThread : public StoppableThread {
    protected:
        void run() override {
            int count = 0;
            while (!stop_requested.load()) {
                // Do some work, like counting up
                count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };

}

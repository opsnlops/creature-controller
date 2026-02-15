
#pragma once

#include "util/StoppableThread.h"
#include <chrono>

/**
 * @brief A simple thread that counts up until it is stopped
 *
 * This may seem silly (because it is), but it's used to test the functionality
 * of StoppableThread.
 *
 */

namespace creatures {

class CountingThread : public StoppableThread {
  protected:
    void run() override {
        while (!stop_requested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

} // namespace creatures

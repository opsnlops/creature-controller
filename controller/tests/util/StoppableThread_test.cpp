#include <gtest/gtest.h>

#include <chrono>

#include "util/CountingThread.h"


TEST(StoppableThreadTest, ThreadStartsAndStops) {
    creatures::CountingThread countingThread;
    countingThread.start();

    // Let's give the thread some time to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Now, request the thread to stop
    countingThread.shutdown();

    // At this point, the destructor of countingThread will join the thread.
    // We trust that if no exceptions are thrown and the program doesn't hang, the test is successful.
    SUCCEED();
}

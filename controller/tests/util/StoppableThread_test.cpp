#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif

#include "util/CountingThread.h"
#include "util/StoppableThread.h"

using namespace std::chrono_literals;

namespace {

using Clock = std::chrono::steady_clock;

/**
 * Takes a little while to wind down after it's told to stop, so a shutdown that
 * doesn't wait for it would return before `finishedRunning` is set.
 */
class SlowToFinishThread : public creatures::StoppableThread {
  public:
    explicit SlowToFinishThread(std::shared_ptr<std::atomic<bool>> finishedRunning)
        : finishedRunning(std::move(finishedRunning)) {}

    ~SlowToFinishThread() override { stopThread(); }

  protected:
    void run() override {
        while (!stop_requested.load()) {
            std::this_thread::sleep_for(5ms);
        }
        std::this_thread::sleep_for(100ms);
        finishedRunning->store(true);
    }

  private:
    std::shared_ptr<std::atomic<bool>> finishedRunning;
};

/**
 * Ignores stop_requested entirely and runs for a fixed time, like a loop stuck
 * in a call that never times out.
 */
class StubbornThread : public creatures::StoppableThread {
  public:
    explicit StubbornThread(std::chrono::milliseconds runFor) : runFor(runFor) {}

    ~StubbornThread() override { stopThread(); }

  protected:
    void run() override { std::this_thread::sleep_for(runFor); }

  private:
    std::chrono::milliseconds runFor;
};

/**
 * Sleeps for a long time between iterations, using waitForStop()
 */
class LongWaitThread : public creatures::StoppableThread {
  public:
    ~LongWaitThread() override { stopThread(); }

    std::atomic<int> iterations{0};

  protected:
    void run() override {
        while (!waitForStop(10s)) {
            iterations++;
        }
    }
};

// Wait for a detached thread to leave run() before its object goes away
void waitUntilFinished(creatures::StoppableThread &thread, std::chrono::milliseconds limit) {
    const auto deadline = Clock::now() + limit;
    while (!thread.isThreadFinished() && Clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }
    // Let threadMain() finish unwinding after it publishes `finished`
    std::this_thread::sleep_for(20ms);
}

} // namespace

TEST(StoppableThreadTest, ThreadStartsAndStops) {
    creatures::CountingThread countingThread;
    countingThread.start();

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(countingThread.isThreadFinished());

    countingThread.shutdown();

    EXPECT_TRUE(countingThread.isThreadFinished());
    EXPECT_FALSE(countingThread.isThreadJoinable());
    EXPECT_FALSE(countingThread.didShutdownTimeOut());
}

TEST(StoppableThreadTest, DefaultShutdownTimeoutIsTwoSeconds) {
    creatures::CountingThread countingThread;
    EXPECT_EQ(countingThread.getShutdownTimeout(), 2000ms);
    EXPECT_EQ(creatures::StoppableThread::DEFAULT_SHUTDOWN_TIMEOUT, 2000ms);
}

TEST(StoppableThreadTest, ShutdownWaitsForRunToReturn) {
    auto finishedRunning = std::make_shared<std::atomic<bool>>(false);
    SlowToFinishThread thread(finishedRunning);
    thread.start();
    std::this_thread::sleep_for(20ms);

    thread.shutdown();

    // run() takes 100 ms to wind down; shutdown must not return before it has
    EXPECT_TRUE(finishedRunning->load());
    EXPECT_TRUE(thread.isThreadFinished());
    EXPECT_FALSE(thread.isThreadJoinable());
    EXPECT_FALSE(thread.didShutdownTimeOut());
}

TEST(StoppableThreadTest, ShutdownIsIdempotent) {
    creatures::CountingThread countingThread;
    countingThread.start();

    countingThread.shutdown();
    countingThread.shutdown();

    EXPECT_FALSE(countingThread.isThreadJoinable());
    EXPECT_FALSE(countingThread.didShutdownTimeOut());
}

TEST(StoppableThreadTest, ShutdownWithoutStartIsSafe) {
    creatures::CountingThread countingThread;
    countingThread.shutdown();

    EXPECT_TRUE(countingThread.isThreadFinished());
    EXPECT_FALSE(countingThread.didShutdownTimeOut());
}

TEST(StoppableThreadTest, StuckThreadIsAbandonedAfterTimeout) {
    StubbornThread thread(600ms);
    thread.setShutdownTimeout(100ms);
    thread.start();

    const auto started = Clock::now();
    thread.shutdown();
    const auto elapsed = Clock::now() - started;

    // shutdown() gives up after its timeout instead of waiting out the thread
    EXPECT_GE(elapsed, 100ms);
    EXPECT_LT(elapsed, 500ms);
    EXPECT_TRUE(thread.didShutdownTimeOut());
    EXPECT_FALSE(thread.isThreadJoinable());
    EXPECT_FALSE(thread.isThreadFinished());

    // Shutting down again doesn't wait again
    const auto secondStart = Clock::now();
    thread.shutdown();
    EXPECT_LT(Clock::now() - secondStart, 50ms);

    // The abandoned thread still holds `this`, so don't destroy it under it
    waitUntilFinished(thread, 2s);
    EXPECT_TRUE(thread.isThreadFinished());
}

TEST(StoppableThreadTest, SlowThreadWithinTimeoutIsJoined) {
    StubbornThread thread(150ms);
    thread.setShutdownTimeout(1000ms);
    thread.start();

    thread.shutdown();

    EXPECT_FALSE(thread.didShutdownTimeOut());
    EXPECT_TRUE(thread.isThreadFinished());
    EXPECT_FALSE(thread.isThreadJoinable());
}

TEST(StoppableThreadTest, DestroyingRunningThreadStopsAndJoinsIt) {
    auto finishedRunning = std::make_shared<std::atomic<bool>>(false);
    {
        auto thread = std::make_unique<SlowToFinishThread>(finishedRunning);
        thread->start();
        std::this_thread::sleep_for(20ms);
        EXPECT_FALSE(finishedRunning->load());
    }

    // The destructor waited for run() to return
    EXPECT_TRUE(finishedRunning->load());
}

TEST(StoppableThreadTest, DestroyingRightAfterStartIsSafe) {
    // The thread may not even have reached run() yet when the destructor starts
    for (int i = 0; i < 50; i++) {
        auto thread = std::make_unique<creatures::CountingThread>();
        thread->start();
        thread.reset();
    }
    SUCCEED();
}

TEST(StoppableThreadTest, WaitForStopWakesImmediatelyOnShutdown) {
    LongWaitThread thread;
    thread.start();
    std::this_thread::sleep_for(20ms);

    const auto started = Clock::now();
    thread.shutdown();

    EXPECT_LT(Clock::now() - started, 500ms);
    EXPECT_FALSE(thread.didShutdownTimeOut());
    EXPECT_EQ(thread.iterations.load(), 0);
}

#if defined(__linux__) || defined(__APPLE__)
namespace {

/**
 * Names itself only in its constructor, like WatchdogThread, and records the
 * name the operating system gave the thread
 */
class ConstructorNamedThread : public creatures::StoppableThread {
  public:
    ConstructorNamedThread() { threadName = "CtorNamed"; }
    ~ConstructorNamedThread() override { stopThread(); }

    std::atomic<bool> recorded{false};
    std::string osName;

  protected:
    void run() override {
        char buffer[64] = {};
        pthread_getname_np(pthread_self(), buffer, sizeof(buffer));
        osName = buffer;
        recorded.store(true);
        while (!stop_requested.load()) {
            std::this_thread::sleep_for(5ms);
        }
    }
};

} // namespace

// Without this, the thread keeps the name of whichever thread started it, which
// on a creature host is the main thread
TEST(StoppableThreadTest, NamesTheThreadBeforeRunning) {
    ConstructorNamedThread thread;
    thread.start();
    const auto deadline = Clock::now() + 1s;
    while (!thread.recorded.load() && Clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(thread.recorded.load());
    thread.shutdown();
    EXPECT_EQ(thread.osName, "CtorNamed");
}
#endif

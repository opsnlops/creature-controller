//
// StoppableThread.h
//

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "creature/thread/ThreadName.h"

namespace creatures {

/**
 * A thread that can be asked to stop, and that waits for itself to finish.
 *
 * shutdown() sets stop_requested and then waits up to the shutdown timeout
 * (2000 ms by default) for run() to return, joining the thread when it does.
 * Every run() loop is expected to check stop_requested often enough to finish
 * well inside that window.
 *
 * If run() is still going when the timeout expires, the thread is logged and
 * detached so that a stuck thread can't hang the process. The service manager
 * restarts the controller on a non-zero exit, so shutdown has to finish in
 * bounded time even when, say, the firmware has gone silent after an ESTOP.
 * A detached thread may still touch its object, so a timeout is a bug in that
 * thread's run() loop, not a normal way to stop.
 *
 * Every subclass must call stopThread() from its own destructor. By the time
 * the base destructor runs, the subclass's members are gone and the object is
 * no longer of the subclass's type, so a thread still running then (or one
 * that hasn't reached run() yet) races with destruction. The base destructor
 * stops the thread as a last resort, but that is not safe on its own.
 */
class StoppableThread {
  public:
    static constexpr std::chrono::milliseconds DEFAULT_SHUTDOWN_TIMEOUT{2000};

    StoppableThread() : stop_requested(false) {}

    StoppableThread(const StoppableThread &) = delete;
    StoppableThread &operator=(const StoppableThread &) = delete;

    virtual ~StoppableThread() { stopThread(); }

    std::string getName() { return threadName; }

    bool isThreadJoinable() { return thread.joinable(); }

    /**
     * True once run() has returned (or if the thread was never started)
     */
    bool isThreadFinished() {
        std::lock_guard<std::mutex> lock(stateMutex);
        return finished;
    }

    /**
     * True if the last shutdown gave up waiting and detached the thread
     */
    bool didShutdownTimeOut() const { return shutdownTimedOut.load(); }

    void setShutdownTimeout(std::chrono::milliseconds timeout) { shutdownTimeout = timeout; }

    std::chrono::milliseconds getShutdownTimeout() const { return shutdownTimeout; }

    virtual void start() {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            finished = false;
        }
        shutdownTimedOut.store(false);
        thread = std::thread(&StoppableThread::threadMain, this);
    }

    virtual void shutdown() { stopThread(); }

  protected:
    virtual void run() = 0; // This is the method that will be called when the thread starts

    /**
     * Ask the thread to stop and wake anything sleeping in waitForStop()
     */
    void requestStop() {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            stop_requested.store(true);
        }
        stateChanged.notify_all();
    }

    /**
     * Sleep for up to `duration`, waking early if a stop is requested.
     *
     * Use this instead of std::this_thread::sleep_for() in run() loops that wait
     * a long time between iterations, so shutdown doesn't have to wait them out.
     *
     * @return true if a stop has been requested
     */
    bool waitForStop(std::chrono::milliseconds duration) {
        std::unique_lock<std::mutex> lock(stateMutex);
        return stateChanged.wait_for(lock, duration, [this] { return stop_requested.load(); });
    }

    /**
     * Ask the thread to stop and wait for it, up to the shutdown timeout.
     *
     * This is what the base shutdown() does. Every subclass calls it from its
     * destructor, so the thread is gone before the subclass is torn down.
     * Calling it again is harmless.
     */
    void stopThread() {
        requestStop();

        // Serialize callers - shutdown() and the destructor may both get here
        std::lock_guard<std::mutex> joinLock(joinMutex);

        if (!thread.joinable()) {
            return;
        }

        // A thread can't join itself. It's already been told to stop, so it'll
        // finish once it unwinds back out of run().
        if (thread.get_id() == std::this_thread::get_id()) {
            thread.detach();
            return;
        }

        bool done;
        {
            std::unique_lock<std::mutex> lock(stateMutex);
            done = stateChanged.wait_for(lock, shutdownTimeout, [this] { return finished; });
        }

        if (done) {
            thread.join();
            return;
        }

        // Give up on it rather than hang the whole process. This is the only
        // place a StoppableThread is ever detached.
        shutdownTimedOut.store(true);
        std::cerr << "StoppableThread: " << threadName << " did not stop within " << shutdownTimeout.count()
                  << " ms; detaching it" << std::endl;
        thread.detach();
    }

    std::atomic<bool> stop_requested;
    std::string threadName = "unnamed thread";

  private:
    void threadMain() {
        // Name the thread now, so a subclass that only sets threadName in its
        // constructor still shows up by name. Subclasses that build their name
        // in run() set it again there.
        setThreadName(threadName);
        run();

        // Notify while holding the lock. Once it's released, the thread waiting
        // in stopThread() is free to join and destroy us.
        std::lock_guard<std::mutex> lock(stateMutex);
        finished = true;
        stateChanged.notify_all();
    }

    std::thread thread;

    std::mutex joinMutex;
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    bool finished = true;
    std::atomic<bool> shutdownTimedOut{false};
    std::chrono::milliseconds shutdownTimeout = DEFAULT_SHUTDOWN_TIMEOUT;
};

} // namespace creatures

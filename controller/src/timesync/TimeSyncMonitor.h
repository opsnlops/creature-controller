#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "creature/logging/Logger.h"
#include "timesync/TimeSync.h"

namespace creatures::timesync {

/**
 * Reads PTP and system clock status. Only Linux can check; elsewhere it's unknown.
 */
class TimeSyncMonitor {
  public:
    explicit TimeSyncMonitor(std::string ptp4lSocket = PTP4L_READ_ONLY_SOCKET);

    TimeSyncStatus check();

  private:
    PtpReading readPtp();
    ClockReading readClock();

    std::string ptp4lSocket;
    uint16_t sequence = 0;
    uint32_t probe = 0;
};

/**
 * Checks sync every few seconds and logs it: when it changes, and again every
 * minute while it's lost. Audio keeps playing either way.
 */
class TimeSyncWatcher {
  public:
    TimeSyncWatcher(std::shared_ptr<Logger> logger, std::unique_ptr<TimeSyncMonitor> monitor);

    /**
     * Check if it's time, and log what changed
     *
     * @return the new status when the sync state changed, otherwise nothing
     */
    std::optional<TimeSyncStatus> poll();

    /**
     * The most recent status, checking now if there isn't one yet
     */
    const TimeSyncStatus &current();

  private:
    std::shared_ptr<Logger> logger;
    std::unique_ptr<TimeSyncMonitor> monitor;
    std::optional<TimeSyncStatus> last;
    std::chrono::steady_clock::time_point nextCheck{};
    std::chrono::steady_clock::time_point nextReminder{};
};

} // namespace creatures::timesync

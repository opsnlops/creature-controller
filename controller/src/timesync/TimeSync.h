#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace creatures::timesync {

// Where ptp4l answers read-only management queries (uds_ro_address)
inline constexpr const char *PTP4L_READ_ONLY_SOCKET = "/var/run/ptp4lro";

// The PTP hardware clock must be this close to the grandmaster: a little over two
// samples at 48 kHz. Healthy hosts run at a few hundred nanoseconds.
inline constexpr int64_t PTP_MAX_OFFSET_NS = 50'000;

// The kernel grows maxerror by 500 microseconds a second until something
// disciplines the clock, so this catches phc2sys going quiet for about 20 seconds
inline constexpr long CLOCK_MAX_ERROR_US = 10'000;

/**
 * What ptp4l reports about following the grandmaster
 */
struct PtpReading {
    bool available = false;
    std::string error; // Why it isn't available
    std::string portState;
    bool gmPresent = false;
    int64_t offsetNs = 0;
    std::string gmIdentity;
};

/**
 * What the kernel reports about the system clock, which phc2sys disciplines
 */
struct ClockReading {
    bool available = false;
    bool synced = false;
    long maxErrorUs = 0;
};

enum class SyncState { Synced, Unsynced, Unknown };

struct TimeSyncStatus {
    SyncState state = SyncState::Unknown;
    std::string reason;
    PtpReading ptp;
    ClockReading clock;
};

const char *syncStateName(SyncState state);

/**
 * Is phc2sys keeping the system clock in step: the kernel's synced flag is set and
 * the clock was corrected recently?
 */
bool clockDisciplined(const ClockReading &clock);

/**
 * Decide whether the host is synced: ptp4l following a grandmaster closely, and the
 * system clock kept in step by phc2sys. RTCP scheduling needs both.
 */
TimeSyncStatus evaluateTimeSync(const PtpReading &ptp, const ClockReading &clock);

/**
 * The status when this platform can't check it
 */
TimeSyncStatus unknownTimeSync(const std::string &reason);

/**
 * Add the sync state to an mDNS TXT record
 */
void appendTimeSyncTxt(std::vector<std::pair<std::string, std::string>> &txt, const TimeSyncStatus &status);

} // namespace creatures::timesync

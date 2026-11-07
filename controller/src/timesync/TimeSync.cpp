#include <cstdlib>

#include <fmt/format.h>

#include "timesync/TimeSync.h"

namespace creatures::timesync {

const char *syncStateName(SyncState state) {
    switch (state) {
    case SyncState::Synced:
        return "synced";
    case SyncState::Unsynced:
        return "unsynced";
    default:
        return "unknown";
    }
}

TimeSyncStatus evaluateTimeSync(const PtpReading &ptp, const ClockReading &clock) {
    TimeSyncStatus status;
    status.ptp = ptp;
    status.clock = clock;
    status.state = SyncState::Unsynced;

    const bool isGrandmaster = ptp.available && ptp.portState == "MASTER";

    if (!ptp.available) {
        status.reason = fmt::format("ptp4l isn't answering: {}", ptp.error);
    } else if (ptp.portState != "SLAVE" && !isGrandmaster) {
        status.reason = fmt::format("the PTP port is {}, not following a grandmaster", ptp.portState);
    } else if (!isGrandmaster && !ptp.gmPresent) {
        status.reason = "ptp4l can't see a grandmaster";
    } else if (!isGrandmaster && std::llabs(ptp.offsetNs) > PTP_MAX_OFFSET_NS) {
        status.reason = fmt::format("the PTP clock is {} ns from the grandmaster, more than {} ns", ptp.offsetNs,
                                    PTP_MAX_OFFSET_NS);
    } else if (!clock.available) {
        status.reason = "unable to read the kernel's clock status";
    } else if (!clock.synced) {
        status.reason = "the system clock isn't synchronized; is phc2sys running?";
    } else if (clock.maxErrorUs > CLOCK_MAX_ERROR_US) {
        status.reason = fmt::format("the system clock hasn't been disciplined recently (max error {} ms); is "
                                    "phc2sys running?",
                                    clock.maxErrorUs / 1000);
    } else {
        status.state = SyncState::Synced;
        status.reason = isGrandmaster ? "this host is the PTP grandmaster"
                                      : fmt::format("following grandmaster {} at {} ns", ptp.gmIdentity, ptp.offsetNs);
    }
    return status;
}

TimeSyncStatus unknownTimeSync(const std::string &reason) {
    TimeSyncStatus status;
    status.state = SyncState::Unknown;
    status.reason = reason;
    return status;
}

void appendTimeSyncTxt(std::vector<std::pair<std::string, std::string>> &txt, const TimeSyncStatus &status) {
    txt.emplace_back("ptp", syncStateName(status.state));
    if (status.ptp.available) {
        txt.emplace_back("ptp_port", status.ptp.portState);
        txt.emplace_back("ptp_gm", status.ptp.gmIdentity);
    }
    if (status.clock.available) {
        txt.emplace_back("clock_synced", status.clock.synced ? "yes" : "no");
    }
}

} // namespace creatures::timesync

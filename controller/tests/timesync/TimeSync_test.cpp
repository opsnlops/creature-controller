#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "timesync/TimeSync.h"

namespace creatures::timesync {

namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Pair;

PtpReading lockedPtp() {
    PtpReading ptp;
    ptp.available = true;
    ptp.portState = "SLAVE";
    ptp.gmPresent = true;
    ptp.offsetNs = 482;
    ptp.gmIdentity = "380525.fffe.33ef92";
    return ptp;
}

ClockReading syncedClock() {
    ClockReading clock;
    clock.available = true;
    clock.synced = true;
    clock.maxErrorUs = 500;
    return clock;
}

TEST(TimeSync, SyncedWhenFollowingAGrandmasterWithADisciplinedClock) {
    const auto status = evaluateTimeSync(lockedPtp(), syncedClock());
    EXPECT_EQ(status.state, SyncState::Synced);
    EXPECT_THAT(status.reason, HasSubstr("380525.fffe.33ef92"));
}

TEST(TimeSync, UnsyncedWhenPtp4lIsntAnswering) {
    PtpReading ptp;
    ptp.error = "no reply";
    const auto status = evaluateTimeSync(ptp, syncedClock());
    EXPECT_EQ(status.state, SyncState::Unsynced);
    EXPECT_THAT(status.reason, HasSubstr("ptp4l isn't answering"));
}

TEST(TimeSync, UnsyncedWhenNotFollowingAGrandmaster) {
    auto ptp = lockedPtp();
    ptp.portState = "LISTENING";
    EXPECT_THAT(evaluateTimeSync(ptp, syncedClock()).reason, HasSubstr("LISTENING"));

    ptp = lockedPtp();
    ptp.gmPresent = false;
    EXPECT_EQ(evaluateTimeSync(ptp, syncedClock()).state, SyncState::Unsynced);
}

TEST(TimeSync, UnsyncedWhenTheOffsetIsTooLarge) {
    auto ptp = lockedPtp();
    ptp.offsetNs = -(PTP_MAX_OFFSET_NS + 1);
    EXPECT_EQ(evaluateTimeSync(ptp, syncedClock()).state, SyncState::Unsynced);

    ptp.offsetNs = PTP_MAX_OFFSET_NS;
    EXPECT_EQ(evaluateTimeSync(ptp, syncedClock()).state, SyncState::Synced);
}

TEST(TimeSync, UnsyncedWhenPhc2sysIsntDiscipliningTheClock) {
    auto clock = syncedClock();
    clock.synced = false;
    EXPECT_THAT(evaluateTimeSync(lockedPtp(), clock).reason, HasSubstr("phc2sys"));

    clock = syncedClock();
    clock.maxErrorUs = CLOCK_MAX_ERROR_US + 1;
    EXPECT_THAT(evaluateTimeSync(lockedPtp(), clock).reason, HasSubstr("hasn't been disciplined"));
}

TEST(TimeSync, TheGrandmasterHostCountsAsSynced) {
    auto ptp = lockedPtp();
    ptp.portState = "MASTER";
    ptp.gmPresent = false;
    EXPECT_EQ(evaluateTimeSync(ptp, syncedClock()).state, SyncState::Synced);
}

TEST(TimeSync, AddsTheStateToTxtRecords) {
    std::vector<std::pair<std::string, std::string>> txt;
    appendTimeSyncTxt(txt, evaluateTimeSync(lockedPtp(), syncedClock()));
    EXPECT_THAT(txt, Contains(Pair("ptp", "synced")));
    EXPECT_THAT(txt, Contains(Pair("ptp_port", "SLAVE")));
    EXPECT_THAT(txt, Contains(Pair("ptp_gm", "380525.fffe.33ef92")));
    EXPECT_THAT(txt, Contains(Pair("clock_synced", "yes")));

    std::vector<std::pair<std::string, std::string>> unknown;
    appendTimeSyncTxt(unknown, unknownTimeSync("not Linux"));
    EXPECT_EQ(unknown, (std::vector<std::pair<std::string, std::string>>{{"ptp", "unknown"}}));
}

} // namespace

} // namespace creatures::timesync

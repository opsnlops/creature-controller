
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include "io/handlers/StatsMessage.h"
#include "io/handlers/StatsHandler.h"
#include "mocks/logging/MockLogger.h"

TEST(StatsHandler, Create) {

    EXPECT_NO_THROW({
                        auto statsHandler = creatures::StatsHandler();
                    });
}

/*
 * There was a bug in the StatsHandler that caused it to fail to parse STATS
 * messages on Linux only. (The bug impacted both macOS and Linux, but it only
 * caused a segfault on Linux.)
 */
TEST(StatsHandler, HandleValid) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    auto tokens = std::vector<std::string>();
    tokens.emplace_back("STATS");
    tokens.emplace_back("FREE_HEAP 20394");

    auto statsHandler = creatures::StatsHandler();

    EXPECT_NO_FATAL_FAILURE(statsHandler.handle(logger, tokens));

}

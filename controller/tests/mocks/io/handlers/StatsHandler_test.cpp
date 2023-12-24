
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

TEST(StatsHandler, HandleValid) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    auto tokens = std::vector<std::string>();
    tokens.emplace_back("STATS");
    tokens.emplace_back("FREE_HEAP 20394");

    EXPECT_NO_THROW({
                        auto statsHandler = creatures::StatsHandler();
                        statsHandler.handle(logger, tokens);
                    });
}

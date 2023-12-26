
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dmx/E131Server.h"
#include "dmx/E131Exception.h"
#include "mocks/logging/MockLogger.h"

TEST(E131Server, Create) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();

    EXPECT_NO_THROW({
        auto server = creatures::dmx::E131Server(logger);
    });
}

TEST(E131Server, StartFailsWithoutController) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto server = creatures::dmx::E131Server(logger);

    EXPECT_THROW(server.start(), creatures::dmx::E131Exception);

}
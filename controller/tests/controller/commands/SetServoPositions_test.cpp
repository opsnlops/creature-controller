
#include <gtest/gtest.h>

#include "mocks/logging/MockLogger.h"

#include "controller/commands/CommandException.h"
#include "controller/commands/SetServoPositions.h"
#include "controller/commands/tokens/ServoPosition.h"

TEST(SetServoPositions, Create) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    // Assert that setServoPositions is not null
    ASSERT_NE(setServoPositions, nullptr) << "setServoPositions should not be null";
}

TEST(SetServoPositions, AddPosition) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
        setServoPositions->addServoPosition(creatures::ServoPosition("A0", 12345));
        setServoPositions->addServoPosition(creatures::ServoPosition("A1", 54321));
    });
}

TEST(SetServoPositions, NoDuplicateOutputPositions) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_THROW(({
        setServoPositions->addServoPosition(creatures::ServoPosition("A0", 12345));
        setServoPositions->addServoPosition(creatures::ServoPosition("A1", 12345));
        setServoPositions->addServoPosition(creatures::ServoPosition("A2", 12345));
        setServoPositions->addServoPosition(creatures::ServoPosition("A0", 54321));
    }), creatures::CommandException);
}

TEST(SetServoPositions, ToMessage) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
        setServoPositions->addServoPosition(creatures::ServoPosition("A0", 123));
        setServoPositions->addServoPosition(creatures::ServoPosition("A1", 456));
        setServoPositions->addServoPosition(creatures::ServoPosition("A2", 789));
        setServoPositions->addServoPosition(creatures::ServoPosition("A3", 012));
    });

    EXPECT_EQ(setServoPositions->toMessage(), "POS\tA0 123\tA1 456\tA2 789\tA3 10");
}

TEST(SetServoPositions, ChecksumValid) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
                        setServoPositions->addServoPosition(creatures::ServoPosition("A0", 123));
                        setServoPositions->addServoPosition(creatures::ServoPosition("A1", 456));
                        setServoPositions->addServoPosition(creatures::ServoPosition("A2", 789));
                        setServoPositions->addServoPosition(creatures::ServoPosition("A3", 012));
                    });

    EXPECT_EQ(1438, setServoPositions->getChecksum());
}

TEST(SetServoPositions, ChecksumInvalid) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
                        setServoPositions->addServoPosition(creatures::ServoPosition("A0", 666));
                        setServoPositions->addServoPosition(creatures::ServoPosition("A3", 0x845));
                    });

    EXPECT_NE(80085, setServoPositions->getChecksum());
}


TEST(SetServoPositions, ToMessageWithChecksum) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
                        setServoPositions->addServoPosition(creatures::ServoPosition("A0", 123));
                        setServoPositions->addServoPosition(creatures::ServoPosition("A1", 456));
                        setServoPositions->addServoPosition(creatures::ServoPosition("A2", 789));
                        setServoPositions->addServoPosition(creatures::ServoPosition("A3", 012));
                    });

    EXPECT_EQ("POS\tA0 123\tA1 456\tA2 789\tA3 10\tCS 1438", setServoPositions->toMessageWithChecksum());
}
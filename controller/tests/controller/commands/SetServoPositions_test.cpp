
#include <gtest/gtest.h>

#include "mocks/logging/MockLogger.h"

#include "controller/commands/CommandException.h"
#include "controller/commands/SetServoPositions.h"
#include "controller/commands/tokens/ServoPosition.h"
#include "device/ServoSpecifier.h"

TEST(SetServoPositions, Create) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    // Assert that setServoPositions is not null
    ASSERT_NE(setServoPositions, nullptr) << "setServoPositions should not be null";
}

TEST(SetServoPositions, AddPosition) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto id1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto id2 = ServoSpecifier(creatures::config::UARTDevice::B, 1);
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
        setServoPositions->addServoPosition(creatures::ServoPosition(id1, 12345));
        setServoPositions->addServoPosition(creatures::ServoPosition(id2, 54321));
    });
}

//TEST(SetServoPositions, NoDuplicateOutputPositions) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//    auto id1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
//    auto id2 = ServoSpecifier(creatures::config::UARTDevice::B, 1);
//    auto id3 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
//    auto id4 = ServoSpecifier(creatures::config::UARTDevice::C, 5);
//    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);
//
//    EXPECT_THROW(({
//        setServoPositions->addServoPosition(creatures::ServoPosition(id1, 12345));
//        setServoPositions->addServoPosition(creatures::ServoPosition(id2, 12345));
//        setServoPositions->addServoPosition(creatures::ServoPosition(id3, 12345));
//        setServoPositions->addServoPosition(creatures::ServoPosition(id4, 54321));
//    }), creatures::CommandException);
//}

TEST(SetServoPositions, ToMessage) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto id1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto id2 = ServoSpecifier(creatures::config::UARTDevice::A, 1);
    auto id3 = ServoSpecifier(creatures::config::UARTDevice::A, 4);
    auto id4 = ServoSpecifier(creatures::config::UARTDevice::A, 5);
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
        setServoPositions->addServoPosition(creatures::ServoPosition(id1, 123));
        setServoPositions->addServoPosition(creatures::ServoPosition(id2, 456));
        setServoPositions->addServoPosition(creatures::ServoPosition(id3, 789));
        setServoPositions->addServoPosition(creatures::ServoPosition(id4, 012));
    });

    EXPECT_EQ(setServoPositions->toMessage(), "POS\t0 123\t1 456\t4 789\t5 10");
}

//TEST(SetServoPositions, ChecksumValid) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//    auto id1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
//    auto id2 = ServoSpecifier(creatures::config::UARTDevice::A, 1);
//    auto id3 = ServoSpecifier(creatures::config::UARTDevice::A, 2);
//    auto id4 = ServoSpecifier(creatures::config::UARTDevice::A, 3);
//    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);
//
//    EXPECT_NO_THROW({
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id1, 123));
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id2, 456));
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id3, 789));
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id4, 012));
//                    });
//
//    EXPECT_EQ(1438, setServoPositions->getChecksum());
//}

TEST(SetServoPositions, ChecksumInvalid) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto id1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto id2 = ServoSpecifier(creatures::config::UARTDevice::A, 3);
    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);

    EXPECT_NO_THROW({
                        setServoPositions->addServoPosition(creatures::ServoPosition(id1, 666));
                        setServoPositions->addServoPosition(creatures::ServoPosition(id2, 0x845));
                    });

    EXPECT_NE(80085, setServoPositions->getChecksum());
}


//TEST(SetServoPositions, ToMessageWithChecksum) {
//
//    auto logger = std::make_shared<creatures::NiceMockLogger>();
//    auto id1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
//    auto id2 = ServoSpecifier(creatures::config::UARTDevice::A, 1);
//    auto id3 = ServoSpecifier(creatures::config::UARTDevice::A, 2);
//    auto id4 = ServoSpecifier(creatures::config::UARTDevice::A, 3);
//    auto setServoPositions = std::make_shared<creatures::commands::SetServoPositions>(logger);
//
//    EXPECT_NO_THROW({
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id1, 123));
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id2, 456));
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id3, 789));
//                        setServoPositions->addServoPosition(creatures::ServoPosition(id4, 012));
//                    });
//
//    EXPECT_EQ("POS\t0 123\t1 456\t2 789\t3 10\tCS 1438", setServoPositions->toMessageWithChecksum());
//}
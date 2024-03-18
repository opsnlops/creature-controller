
#include <gtest/gtest.h>

#include "creature/Creature.h"
#include "creature/Parrot.h"
#include "creature/CreatureException.h"

#include "mocks/logging/MockLogger.h"

TEST(Creature, CreateParrot) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    std::shared_ptr<creatures::creature::Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    EXPECT_EQ("doug", parrot->getName());

}

TEST(Creature, ServoMap_BaseFunctionality) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto location1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto location2 = ServoSpecifier(creatures::config::UARTDevice::B, 1);
    auto location3 = ServoSpecifier(creatures::config::UARTDevice::C, 3);
    auto location4 = ServoSpecifier(creatures::config::UARTDevice::A, 1);

    std::shared_ptr<creatures::creature::Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    parrot->addServo("a", std::make_shared<Servo>(logger, "a", "Servo A0", location1, 1000, 3000, 0.90, false, 50, 2000));
    parrot->addServo("b", std::make_shared<Servo>(logger, "b", "Servo B1", location2, 1000, 3000, 0.90, false, 50, 2000));
    parrot->addServo("c", std::make_shared<Servo>(logger, "c", "Servo C3", location3, 1000, 3000, 0.90, false, 50, 2000));
    parrot->addServo("d", std::make_shared<Servo>(logger, "d", "Servo A2", location4, 1000, 3000, 0.90, false, 50, 2000));

    EXPECT_EQ(4, parrot->getNumberOfServos());

    EXPECT_EQ("Servo A0", parrot->getServo("a")->getName());
    EXPECT_EQ("Servo B1", parrot->getServo("b")->getName());
    EXPECT_EQ("Servo C3", parrot->getServo("c")->getName());
    EXPECT_EQ("Servo A2", parrot->getServo("d")->getName());

    EXPECT_EQ(nullptr, parrot->getServo("e"));
}

TEST(Creature, ServoMap_DuplicateId) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    auto location1 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto location2 = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    std::shared_ptr<creatures::creature::Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    parrot->addServo("a", std::make_shared<Servo>(logger, "a", "Servo A3", location1, 1000, 3000, 0.90, false, 50, 2000));

    EXPECT_THROW({parrot->addServo("a", std::make_shared<Servo>(logger, "a", "Servo B (but a)", location2, 1000, 3000, 0.90, false, 50, 2000));
                 }, creatures::CreatureException);

}
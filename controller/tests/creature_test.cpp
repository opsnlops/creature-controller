
#include <gtest/gtest.h>

#include "creature/Creature.h"
#include "creature/Parrot.h"
#include "creature/CreatureException.h"

#include "mocks/logging/MockLogger.h"

TEST(Creature, CreateParrot) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    std::shared_ptr<Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    EXPECT_EQ("doug", parrot->getName());

}

TEST(Creature, ServoMap_BaseFunctionality) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    std::shared_ptr<Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    parrot->addServo("a", std::make_shared<Servo>(logger, "a", "A0", "Servo A0", 1000, 3000, 0.90, false, 50, 2000));
    parrot->addServo("b", std::make_shared<Servo>(logger, "b", "B1", "Servo B1", 1000, 3000, 0.90, false, 50, 2000));
    parrot->addServo("c", std::make_shared<Servo>(logger, "c", "C3", "Servo C3", 1000, 3000, 0.90, false, 50, 2000));
    parrot->addServo("d", std::make_shared<Servo>(logger, "d", "A2", "Servo A2", 1000, 3000, 0.90, false, 50, 2000));

    EXPECT_EQ(4, parrot->getNumberOfServos());

    EXPECT_EQ("Servo A0", parrot->getServo("a")->getName());
    EXPECT_EQ("Servo B1", parrot->getServo("b")->getName());
    EXPECT_EQ("Servo C3", parrot->getServo("c")->getName());
    EXPECT_EQ("Servo A2", parrot->getServo("d")->getName());

    EXPECT_EQ(nullptr, parrot->getServo("e"));
}

TEST(Creature, ServoMap_DuplicateId) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    std::shared_ptr<Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    parrot->addServo("a", std::make_shared<Servo>(logger, "a", "A3", "Servo A3", 1000, 3000, 0.90, false, 50, 2000));

    EXPECT_THROW({parrot->addServo("a", std::make_shared<Servo>(logger, "a", "A0", "Servo B (but a)", 1000, 3000, 0.90, false, 50, 2000));
                 }, creatures::CreatureException);

}
//
// Created by April White on 12/10/23.
//

#include <gtest/gtest.h>

#include "creature/Creature.h"
#include "creature/Parrot.h"
#include "creature/CreatureException.h"

#include "MockLogger.h"

TEST(Creature, CreateParrot) {

    auto logger = std::make_shared<creatures::MockLogger>();
    std::shared_ptr<Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    EXPECT_EQ("doug", parrot->getName());

}

TEST(Creature, ServoMap_BaseFunctionality) {

    auto logger = std::make_shared<creatures::NiceMockLogger>();
    std::shared_ptr<Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    parrot->addServo("a", std::make_shared<Servo>(logger, "a", 0, "Servo A", 1000, 3000, 0.90, false, 2000));
    parrot->addServo("b", std::make_shared<Servo>(logger,"b", 0, "Servo B", 1000, 3000, 0.90, false, 2000));
    parrot->addServo("c", std::make_shared<Servo>(logger,"c", 0, "Servo C", 1000, 3000, 0.90, false, 2000));
    parrot->addServo("d", std::make_shared<Servo>(logger,"d", 0, "Servo D", 1000, 3000, 0.90, false, 2000));

    EXPECT_EQ(4, parrot->getNumberOfServos());

    EXPECT_EQ("Servo A", parrot->getServo("a")->getName());
    EXPECT_EQ("Servo B", parrot->getServo("b")->getName());
    EXPECT_EQ("Servo C", parrot->getServo("c")->getName());
    EXPECT_EQ("Servo D", parrot->getServo("d")->getName());

    EXPECT_EQ(nullptr, parrot->getServo("e"));
}

TEST(Creature, ServoMap_DuplicateId) {

    auto logger = std::make_shared<creatures::MockLogger>();
    std::shared_ptr<Creature> parrot = std::make_shared<Parrot>(logger);
    parrot->setName("doug");

    parrot->addServo("a", std::make_shared<Servo>(logger, "a", 0, "Servo A", 1000, 3000, 0.90, false, 2000));

    EXPECT_THROW({parrot->addServo("a", std::make_shared<Servo>(logger, "a", 0, "Servo B (but a)", 1000, 3000, 0.90, false, 2000));
                 }, creatures::CreatureException);

}
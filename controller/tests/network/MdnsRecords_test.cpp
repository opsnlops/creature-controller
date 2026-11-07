#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mocks/creature/MockCreature.h"
#include "mocks/logging/MockLogger.h"
#include "network/MdnsRecords.h"

namespace creatures::network {

namespace {

using ::testing::Contains;
using ::testing::Pair;

TEST(MdnsRecords, InstanceNameIsLowerCase) {
    EXPECT_EQ(mdnsInstanceName("Caroll"), "caroll");
    EXPECT_EQ(mdnsInstanceName("BEAKY"), "beaky");
    EXPECT_EQ(mdnsInstanceName("Light 1"), "light 1");
}

TEST(MdnsRecords, DescribesTheCreature) {
    auto logger = std::make_shared<NiceMockLogger>();
    MockCreature creature(logger);
    creature.setName("Caroll");
    creature.setId("56665920-98f7-11f1-9c92-2ccf67b00612");
    creature.setType(creature::Creature::creature_type::parrot);
    creature.setVersion("0.1.0");
    creature.setChannelOffset(50);
    creature.setAudioChannel(5);

    const auto service = makeCreatureService(creature, 2, "4.6.1", 5568);

    EXPECT_EQ(service.name, "caroll");
    EXPECT_EQ(service.type, "_creature-controller._udp");
    EXPECT_EQ(service.port, 5568);
    EXPECT_THAT(service.txt, Contains(Pair("txtvers", "1")));
    EXPECT_THAT(service.txt, Contains(Pair("id", "56665920-98f7-11f1-9c92-2ccf67b00612")));
    EXPECT_THAT(service.txt, Contains(Pair("name", "Caroll")));
    EXPECT_THAT(service.txt, Contains(Pair("type", "parrot")));
    EXPECT_THAT(service.txt, Contains(Pair("version", "0.1.0")));
    EXPECT_THAT(service.txt, Contains(Pair("universe", "2")));
    EXPECT_THAT(service.txt, Contains(Pair("channel_offset", "50")));
    EXPECT_THAT(service.txt, Contains(Pair("audio_channel", "5")));
    EXPECT_THAT(service.txt, Contains(Pair("controller_version", "4.6.1")));
}

TEST(MdnsRecords, CreatureTypeRoundTrips) {
    using creature::Creature;
    for (auto type : {Creature::creature_type::parrot, Creature::creature_type::crow}) {
        EXPECT_EQ(Creature::stringToCreatureType(Creature::creatureTypeToString(type)), type);
    }
}

} // namespace

} // namespace creatures::network

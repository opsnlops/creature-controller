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

class MdnsRecordsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        creature.setName("Caroll");
        creature.setId("56665920-98f7-11f1-9c92-2ccf67b00612");
        creature.setType(creature::Creature::creature_type::parrot);
        creature.setVersion("0.1.0");
        creature.setDescription("A parrot");
        creature.setChannelOffset(50);
        creature.setAudioChannel(5);
        creature.setMouthSlot(4);
        creature.setServoUpdateFrequencyHz(50);
        creature.setPositionMin(0);
        creature.setPositionMax(1023);
        creature.setPowerDrawLimits(15.0, 14.0, 10.0);

        details.controllerVersion = "4.6.2";
        details.port = 5568;
        details.universe = 2;
        details.networkInterface = "eth0";
        details.serialDevice = "/dev/creature/Caroll";
        details.rtpAudio = true;
        details.audioDevice = "plughw:CARD=BossDAC,DEV=0";
        details.server = "10.19.63.5:8000";
        details.temperatureLimitF = 165.0;
    }

    std::shared_ptr<NiceMockLogger> logger = std::make_shared<NiceMockLogger>();
    MockCreature creature{logger};
    ControllerDetails details;
};

TEST_F(MdnsRecordsTest, DescribesTheCreatureAndHost) {
    const auto service = makeCreatureService(creature, details);

    EXPECT_EQ(service.name, "caroll");
    EXPECT_EQ(service.type, "_creature-controller._udp");
    EXPECT_EQ(service.port, 5568);
    EXPECT_THAT(service.txt, Contains(Pair("txtvers", "1")));
    EXPECT_THAT(service.txt, Contains(Pair("id", "56665920-98f7-11f1-9c92-2ccf67b00612")));
    EXPECT_THAT(service.txt, Contains(Pair("name", "Caroll")));
    EXPECT_THAT(service.txt, Contains(Pair("type", "parrot")));
    EXPECT_THAT(service.txt, Contains(Pair("version", "0.1.0")));
    EXPECT_THAT(service.txt, Contains(Pair("description", "A parrot")));
    EXPECT_THAT(service.txt, Contains(Pair("controller_version", "4.6.2")));
    EXPECT_THAT(service.txt, Contains(Pair("universe", "2")));
    EXPECT_THAT(service.txt, Contains(Pair("channel_offset", "50")));
    EXPECT_THAT(service.txt, Contains(Pair("audio_channel", "5")));
    EXPECT_THAT(service.txt, Contains(Pair("mouth_slot", "4")));
    EXPECT_THAT(service.txt, Contains(Pair("servo_hz", "50")));
    EXPECT_THAT(service.txt, Contains(Pair("position_max", "1023")));
    EXPECT_THAT(service.txt, Contains(Pair("power_limit_w", "15")));
    EXPECT_THAT(service.txt, Contains(Pair("power_warning_w", "14")));
    EXPECT_THAT(service.txt, Contains(Pair("power_response_s", "10")));
    EXPECT_THAT(service.txt, Contains(Pair("temp_limit_f", "165")));
    EXPECT_THAT(service.txt, Contains(Pair("serial_device", "/dev/creature/Caroll")));
    EXPECT_THAT(service.txt, Contains(Pair("interface", "eth0")));
    EXPECT_THAT(service.txt, Contains(Pair("rtp_audio", "on")));
    EXPECT_THAT(service.txt, Contains(Pair("audio_device", "plughw:CARD=BossDAC,DEV=0")));
    EXPECT_THAT(service.txt, Contains(Pair("server", "10.19.63.5:8000")));
}

TEST_F(MdnsRecordsTest, FirmwareVersionAppearsOnceKnown) {
    EXPECT_THAT(makeCreatureService(creature, details).txt, Contains(Pair("firmware_version", "unknown")));

    details.firmwareVersion = 4;
    EXPECT_THAT(makeCreatureService(creature, details).txt, Contains(Pair("firmware_version", "4")));
}

TEST_F(MdnsRecordsTest, FitsInOnePacket) {
    auto service = makeCreatureService(creature, details);
    EXPECT_LE(fitTxtRecord(service), MDNS_TXT_RECORD_TARGET_BYTES);
}

TEST(MdnsRecords, CreatureTypeRoundTrips) {
    using creature::Creature;
    for (auto type : {Creature::creature_type::parrot, Creature::creature_type::crow}) {
        EXPECT_EQ(Creature::stringToCreatureType(Creature::creatureTypeToString(type)), type);
    }
}

} // namespace

} // namespace creatures::network

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "audio-router/RouterMdns.h"

namespace creatures::audio::router {

namespace {

using ::testing::Contains;
using ::testing::Pair;

RouterConfig makeConfig() {
    RouterConfig config;
    config.networkInterface = "eth0";
    config.engine.output.deviceName = "hdmi:CARD=vc4hdmi0,DEV=0";
    config.engine.output.channels = {parseSpeakerChannel("front_left"), parseSpeakerChannel("front_right"),
                                     parseSpeakerChannel("lfe")};
    config.engine.output.commonPlayoutDelayMs = 20;
    config.engine.output.audioDeviceCompensationMs = 0;
    config.engine.output.limiterCeilingDb = -1.0f;
    config.engine.inputs = {{1, 0.0f}, {2, 0.0f}, {17, -6.0f}};
    config.engine.routes = {{0, 0, 0.0f}, {1, 1, 0.0f}, {2, 0, -3.0f}};
    config.engine.requireRtcp = true;
    return config;
}

TEST(RouterMdns, DescribesTheRouter) {
    const auto service = makeRouterService("Server-HDMI0", makeConfig(), "0.1.3");

    EXPECT_EQ(service.name, "server-hdmi0");
    EXPECT_EQ(service.type, "_creature-audio-router._udp");
    EXPECT_EQ(service.port, rtp::RTP_PORT);
    EXPECT_THAT(service.txt, Contains(Pair("txtvers", "1")));
    EXPECT_THAT(service.txt, Contains(Pair("version", "0.1.3")));
    EXPECT_THAT(service.txt, Contains(Pair("interface", "eth0")));
    EXPECT_THAT(service.txt, Contains(Pair("device", "hdmi:CARD=vc4hdmi0,DEV=0")));
    EXPECT_THAT(service.txt, Contains(Pair("outputs", "front_left,front_right,lfe")));
    EXPECT_THAT(service.txt, Contains(Pair("inputs", "1,2,17")));
    EXPECT_THAT(service.txt, Contains(Pair("input_gains_db", "0,0,-6")));
    EXPECT_THAT(service.txt, Contains(Pair("routes", "1>front_left,2>front_right,17>front_left@-3")));
    EXPECT_THAT(service.txt, Contains(Pair("bass", "off")));
    EXPECT_THAT(service.txt, Contains(Pair("playout_delay_ms", "20")));
    EXPECT_THAT(service.txt, Contains(Pair("device_compensation_ms", "0")));
    EXPECT_THAT(service.txt, Contains(Pair("limiter_ceiling_db", "-1")));
    EXPECT_THAT(service.txt, Contains(Pair("require_rtcp", "yes")));
}

TEST(RouterMdns, DescribesBassManagement) {
    auto config = makeConfig();
    BassConfig bass;
    bass.output = 2;
    bass.crossoverHz = 80.0f;
    config.engine.bass = bass;

    EXPECT_THAT(makeRouterService("server", config, "0.1.3").txt, Contains(Pair("bass", "lfe@80Hz")));
}

TEST(RouterMdns, NamesTheDeviceNumberWithoutADeviceName) {
    auto config = makeConfig();
    config.engine.output.deviceName.reset();
    config.engine.output.deviceNumber = 1;

    EXPECT_THAT(makeRouterService("server", config, "0.1.3").txt, Contains(Pair("device", "device 1")));
}

} // namespace

} // namespace creatures::audio::router

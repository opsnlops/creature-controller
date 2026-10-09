#include "audio-router/RouterConfig.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
using namespace creatures::audio;
using namespace creatures::audio::router;
namespace {
nlohmann::json configJson() {
    return nlohmann::json::parse(R"({
        "networkInterface":"eth0",
        "output":{"deviceName":"hdmi:CARD=vc4hdmi0,DEV=0","channels":["front_left","front_right","lfe"]},
        "inputs":[{"channel":3},{"channel":17,"gainDb":-6}],
        "routes":[{"input":3,"output":"front_left"},{"input":17,"output":"front_right"}],
        "bass":{"output":"lfe","inputs":[{"channel":17}],"highPassOutputs":["front_right"]}
    })");
}
} // namespace
TEST(RouterConfigTest, UsesNamedRoutesAndDefaultsToRequiredRtcp) {
    const auto config = parseRouterConfig(configJson());
    EXPECT_TRUE(config.engine.requireRtcp);
    EXPECT_EQ(config.engine.routes[1].input, 1);
    EXPECT_EQ(config.engine.output.channels[2], SpeakerChannel::Lfe);
    EXPECT_EQ(config.engine.bass->sends.size(), 1);
}
TEST(RouterConfigTest, RejectsUnknownFieldsAndInvalidTypes) {
    auto json = configJson();
    json["outpt"] = 1;
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json = configJson();
    json["output"]["commonPlayoutDelayMs"] = 20.5;
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json = configJson();
    json["requireRtcp"] = "true";
    EXPECT_THROW((void)parseRouterConfig(json), nlohmann::json::exception);
}
TEST(RouterConfigTest, RejectsDuplicateChannelsAndUnsubscribedRoutes) {
    auto json = configJson();
    json["inputs"].push_back({{"channel", 3}});
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json = configJson();
    json["routes"][0]["input"] = 4;
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json = configJson();
    json["output"]["channels"].push_back("lfe");
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
}
TEST(RouterConfigTest, ValidatesCompensationHeadroomAndBgmMaster) {
    auto json = configJson();
    json["output"]["audioDeviceCompensationMs"] = 10;
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json["output"]["commonPlayoutDelayMs"] = 30;
    EXPECT_NO_THROW((void)parseRouterConfig(json));
    json = configJson();
    json["inputs"][1]["channel"] = 4;
    json["routes"] = nlohmann::json::array();
    json.erase("bass");
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
}
TEST(RouterConfigTest, RejectsDirectRoutesToFilteredBassOutputAndDuplicateBassSends) {
    auto json = configJson();
    json["routes"].push_back({{"input", 17}, {"output", "lfe"}});
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json = configJson();
    json["bass"]["inputs"].push_back({{"channel", 17}});
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
}
TEST(RouterConfigTest, LargeNumbersCannotWrapIntoValidConfiguration) {
    auto json = configJson();
    json["inputs"][0]["channel"] = 259;
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json = configJson();
    json["rtpPort"] = 65536;
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
    json = configJson();
    json["output"]["outputVolumePercent"] = -1;
    EXPECT_THROW((void)parseRouterConfig(json), std::invalid_argument);
}

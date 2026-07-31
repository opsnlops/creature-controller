#include "config/Configuration.h"
#include "config/UARTDevice.h"
#include "mocks/logging/MockLogger.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using namespace creatures;
using namespace creatures::config;

class TestableConfiguration : public Configuration {
  public:
    using Configuration::Configuration; // Inherit constructors
    // Expose protected methods for testing
    using Configuration::addUARTDevice;
    using Configuration::setNetworkDeviceName;
    using Configuration::setUseGPIO;
};

class ConfigurationTest : public ::testing::Test {
  protected:
    std::shared_ptr<NiceMockLogger> mockLogger;
    std::unique_ptr<TestableConfiguration> config;

    ConfigurationTest() : mockLogger(std::make_shared<NiceMockLogger>()) {}

    void SetUp() override { config = std::make_unique<TestableConfiguration>(mockLogger); }
};

TEST_F(ConfigurationTest, SetAndGetCreatureConfigFile) {
    std::string fileName = "creature_config.json";
    config->setCreatureConfigFile(fileName);
    ASSERT_EQ(config->getCreatureConfigFile(), fileName);
}

// Removed SetAndGetUsbDevice test since it doesn't match existing methods in Configuration

TEST_F(ConfigurationTest, SetAndGetUseGPIO) {
    config->setUseGPIO(true);
    ASSERT_TRUE(config->getUseGPIO());

    config->setUseGPIO(false);
    ASSERT_FALSE(config->getUseGPIO());
}

TEST_F(ConfigurationTest, SetAndGetNetworkDeviceName) {
    std::string deviceName = "eth1";
    config->setNetworkDeviceName(deviceName);
    ASSERT_EQ(config->getNetworkDeviceName(), deviceName);
}

TEST_F(ConfigurationTest, AudioConfigHasSafeDefaults) {
    const auto &audioConfig = config->getAudioConfig();

    EXPECT_EQ(audioConfig.deviceNumber, creatures::audio::DEFAULT_SOUND_DEVICE_NUMBER);
    EXPECT_FALSE(audioConfig.deviceName.has_value());
    EXPECT_FLOAT_EQ(audioConfig.dialogGainDb, creatures::audio::DEFAULT_DIALOG_GAIN_DB);
    EXPECT_FLOAT_EQ(audioConfig.bgmGainDb, creatures::audio::DEFAULT_BGM_GAIN_DB);
    EXPECT_FLOAT_EQ(audioConfig.limiterCeilingDb, creatures::audio::DEFAULT_LIMITER_CEILING_DB);
    EXPECT_FALSE(audioConfig.outputVolumePercent.has_value());
    EXPECT_EQ(audioConfig.commonPlayoutDelayMs, creatures::audio::DEFAULT_COMMON_PLAYOUT_DELAY_MS);
    EXPECT_EQ(audioConfig.audioDeviceCompensationMs, creatures::audio::DEFAULT_AUDIO_DEVICE_COMPENSATION_MS);
}

TEST_F(ConfigurationTest, SetsAudioConfig) {
    config->setSoundDeviceNumber(2);
    config->setSoundDeviceName("plughw:CARD=S3,DEV=0");
    config->setDialogGainDb(1.5f);
    config->setBgmGainDb(-8.0f);
    config->setLimiterCeilingDb(-2.0f);
    config->setOutputVolumePercent(72);
    config->setAlsaMixerCard("hw:1");
    config->setAlsaMixerElement("PCM");
    config->setCommonPlayoutDelayMs(20);
    config->setAudioDeviceCompensationMs(3);

    const auto &audioConfig = config->getAudioConfig();
    EXPECT_EQ(audioConfig.deviceNumber, 2);
    ASSERT_TRUE(audioConfig.deviceName.has_value());
    EXPECT_EQ(*audioConfig.deviceName, "plughw:CARD=S3,DEV=0");
    EXPECT_FLOAT_EQ(audioConfig.dialogGainDb, 1.5f);
    EXPECT_FLOAT_EQ(audioConfig.bgmGainDb, -8.0f);
    EXPECT_FLOAT_EQ(audioConfig.limiterCeilingDb, -2.0f);
    ASSERT_TRUE(audioConfig.outputVolumePercent.has_value());
    EXPECT_EQ(*audioConfig.outputVolumePercent, 72);
    EXPECT_EQ(audioConfig.alsaMixerCard, "hw:1");
    EXPECT_EQ(audioConfig.alsaMixerElement, "PCM");
    EXPECT_EQ(audioConfig.commonPlayoutDelayMs, 20);
    EXPECT_EQ(audioConfig.audioDeviceCompensationMs, 3);
}

TEST_F(ConfigurationTest, AddAndGetUARTDevices) {
    UARTDevice device1(mockLogger);
    UARTDevice device2(mockLogger);

    config->addUARTDevice(device1);
    config->addUARTDevice(device2);

    auto devices = config->getUARTDevices();
    ASSERT_EQ(devices.size(), 2);
}

TEST_F(ConfigurationTest, SetAndGetDynamixelTemperatureLimitDegrees) {
    config->setDynamixelTemperatureLimitDegrees(150.0);
    ASSERT_DOUBLE_EQ(config->getDynamixelTemperatureLimitDegrees(), 150.0);
}

TEST_F(ConfigurationTest, SetAndGetDynamixelTemperatureWarningDegrees) {
    config->setDynamixelTemperatureWarningDegrees(140.0);
    ASSERT_DOUBLE_EQ(config->getDynamixelTemperatureWarningDegrees(), 140.0);
}

TEST_F(ConfigurationTest, SetAndGetDynamixelTemperatureLimitSeconds) {
    config->setDynamixelTemperatureLimitSeconds(10.0);
    ASSERT_DOUBLE_EQ(config->getDynamixelTemperatureLimitSeconds(), 10.0);
}

TEST_F(ConfigurationTest, SetAndGetDynamixelLoadLimitPercent) {
    config->setDynamixelLoadLimitPercent(95.0);
    ASSERT_DOUBLE_EQ(config->getDynamixelLoadLimitPercent(), 95.0);
}

TEST_F(ConfigurationTest, SetAndGetDynamixelLoadWarningPercent) {
    config->setDynamixelLoadWarningPercent(80.0);
    ASSERT_DOUBLE_EQ(config->getDynamixelLoadWarningPercent(), 80.0);
}

TEST_F(ConfigurationTest, SetAndGetDynamixelLoadLimitSeconds) {
    config->setDynamixelLoadLimitSeconds(5.0);
    ASSERT_DOUBLE_EQ(config->getDynamixelLoadLimitSeconds(), 5.0);
}

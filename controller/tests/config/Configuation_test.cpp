#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "config/Configuration.h"
#include "config/UARTDevice.h"
#include "mocks/logging/MockLogger.h"

using ::testing::_;
using ::testing::NiceMock;
using namespace creatures;
using namespace creatures::config;

class TestableConfiguration : public Configuration {
public:
    using Configuration::Configuration; // Inherit constructors
    // Expose protected methods for testing
    using Configuration::setUseGPIO;
    using Configuration::setNetworkDeviceIPAddress;
    using Configuration::addUARTDevice;
};

class ConfigurationTest : public ::testing::Test {
protected:
    std::shared_ptr<NiceMockLogger> mockLogger;
    std::unique_ptr<TestableConfiguration> config;

    ConfigurationTest() : mockLogger(std::make_shared<NiceMockLogger>()) {}

    void SetUp() override {
        config = std::make_unique<TestableConfiguration>(mockLogger);
    }
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

TEST_F(ConfigurationTest, SetAndGetNetworkDeviceIPAddress) {
    std::string ipAddress = "192.168.1.100";
    config->setNetworkDeviceIPAddress(ipAddress);
    ASSERT_EQ(config->getNetworkDeviceIPAddress(), ipAddress);
}

TEST_F(ConfigurationTest, AddAndGetUARTDevices) {
    UARTDevice device1(mockLogger);
    UARTDevice device2(mockLogger);

    config->addUARTDevice(device1);
    config->addUARTDevice(device2);

    auto devices = config->getUARTDevices();
    ASSERT_EQ(devices.size(), 2);
}

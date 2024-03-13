
#include <gtest/gtest.h>

#include "config/UARTDevice.h"
#include "mocks/logging/MockLogger.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrEq;
using namespace creatures;
using namespace creatures::config;

class TestableUARTDevice : public UARTDevice {
public:
    using UARTDevice::UARTDevice; // Inherit constructors
    // Expose protected methods as public for testing
    using UARTDevice::setDeviceNode;
    using UARTDevice::setModule;
    using UARTDevice::setEnabled;
};

class UARTDeviceTest : public ::testing::Test {
protected:
    std::shared_ptr<NiceMockLogger> mockLogger;
    std::unique_ptr<TestableUARTDevice> uartDevice;

    UARTDeviceTest() : mockLogger(std::make_shared<NiceMockLogger>()) {}

    void SetUp() override {
        uartDevice = std::make_unique<TestableUARTDevice>(mockLogger);
    }
};

TEST_F(UARTDeviceTest, ConstructorInitializesDisabledState) {
    ASSERT_FALSE(uartDevice->getEnabled());
}

TEST_F(UARTDeviceTest, DestructorLogsDebugMessage) {
    EXPECT_CALL(*mockLogger, logDebug(StrEq("destroyed a UARTDevice"), _));
    uartDevice.reset();  // Forces the destructor to be called
}

TEST_F(UARTDeviceTest, CopyConstructorCopiesAllFields) {
    uartDevice->setDeviceNode("/dev/ttyAMA0");
    uartDevice->setModule(UARTDevice::module_name::A);
    uartDevice->setEnabled(true);

    UARTDevice copiedDevice = *uartDevice;
    ASSERT_EQ(copiedDevice.getDeviceNode(), "/dev/ttyAMA0");
    ASSERT_EQ(copiedDevice.getModule(), UARTDevice::module_name::A);
    ASSERT_TRUE(copiedDevice.getEnabled());
}

TEST_F(UARTDeviceTest, SetAndGetDeviceNode) {
    std::string testNode = "/dev/ttyAMA0";
    uartDevice->setDeviceNode(testNode);
    ASSERT_EQ(uartDevice->getDeviceNode(), testNode);
}

TEST_F(UARTDeviceTest, SetAndGetModule) {
    uartDevice->setModule(UARTDevice::module_name::B);
    ASSERT_EQ(uartDevice->getModule(), UARTDevice::module_name::B);
}

TEST_F(UARTDeviceTest, SetAndGetEnabled) {
    uartDevice->setEnabled(true);
    ASSERT_TRUE(uartDevice->getEnabled());

    uartDevice->setEnabled(false);
    ASSERT_FALSE(uartDevice->getEnabled());
}


#include <gtest/gtest.h>
#include "io/Message.h"

using creatures::io::Message;
using creatures::config::UARTDevice;


TEST(MessageConstructorTest, ConstructorSetsModuleAndPayload) {
    // Test that the constructor correctly sets the module and payload
    Message testMessage(UARTDevice::module_name::A, "Hello, Bunny!");
    EXPECT_EQ(testMessage.module, UARTDevice::module_name::A);
    EXPECT_EQ(testMessage.payload, "Hello, Bunny!");
}

TEST(MessageModuleTest, SetAndGetModule) {
    // Test setting and getting the module
    Message testMessage(UARTDevice::module_name::B, "");
    testMessage.module = UARTDevice::module_name::C;
    EXPECT_EQ(testMessage.module, UARTDevice::module_name::C);
}

TEST(MessagePayloadTest, SetAndGetPayload) {
    // Test setting and getting the payload
    Message testMessage(UARTDevice::module_name::A, "Initial payload");
    testMessage.payload = "Updated payload";
    EXPECT_EQ(testMessage.payload, "Updated payload");
}

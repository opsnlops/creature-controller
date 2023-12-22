
#include <gtest/gtest.h>
#include "mocks/controller/commands/MockCommand.h"

TEST(ICommandTest, ChecksumCalculation) {
    MockCommand mockCommand;
    std::string testMessage = "MOVE\t23423 A0\tasdfasdfasdf\t";

    // Set up expectation: toMessage() should be called and will return testMessage
    EXPECT_CALL(mockCommand, toMessage())
            .WillOnce(testing::Return(testMessage));

    // Calculate checksum
    u16 checksum = mockCommand.getChecksum();

    // Verify checksum calculation
    u16 expectedChecksum = 0;
    for (char c : testMessage) {
        expectedChecksum += c;
    }

    // Log and verify checksum calculation
    EXPECT_EQ(checksum, expectedChecksum) << "Calculated checksum: " << checksum
                                          << ", Expected checksum: " << expectedChecksum;
}

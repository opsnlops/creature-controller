#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "timesync/PtpManagement.h"

namespace creatures::timesync {

namespace {

std::vector<uint8_t> fromHex(const std::string &hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

// Captured from ptp4l 4.2 on Caroll through its read-only socket
const std::string TIME_STATUS_REPLY =
    "0d1200680000000000000000000000000000000098fe54fffe17e12100000000007f000000000000002a00000000020000010034"
    "c00000000000000001e218e5e5fdb81d1ee20000000000000000000000000000000000000000000000000001380525fffe33ef92";
const std::string PORT_DATA_SET_REPLY = "0d1200500000000000000000000000000000000098fe54fffe17e12100010001007f00000000"
                                        "0000002a0000000002000001001c200498fe54fffe17e121000109000000000000000000010300"
                                        "010002";
// The request that got those replies
const std::string TIME_STATUS_REQUEST = "0d02003600000000000000000000000000000000000000000000002a00000000047ffffffff"
                                        "fffffffffffff0000000000010002c000";

TEST(PtpManagement, EncodesTheSameGetAsTheWorkingRequest) {
    EXPECT_EQ(encodeManagementGet(PTP_MGMT_TIME_STATUS_NP, 0, 42), fromHex(TIME_STATUS_REQUEST));
}

TEST(PtpManagement, DecodesARealTimeStatusReply) {
    const auto bytes = fromHex(TIME_STATUS_REPLY);
    const auto response = decodeManagementResponse(bytes.data(), bytes.size());
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->managementId, PTP_MGMT_TIME_STATUS_NP);

    const auto status = decodeTimeStatus(response->data);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status->masterOffsetNs, 482);
    EXPECT_TRUE(status->gmPresent);
    EXPECT_EQ(status->gmIdentity, "380525.fffe.33ef92");
}

TEST(PtpManagement, DecodesARealPortDataSetReply) {
    const auto bytes = fromHex(PORT_DATA_SET_REPLY);
    const auto response = decodeManagementResponse(bytes.data(), bytes.size());
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->managementId, PTP_MGMT_PORT_DATA_SET);
    EXPECT_EQ(decodePortState(response->data), "SLAVE");
}

TEST(PtpManagement, RejectsRequestsAndTruncatedMessages) {
    const auto request = fromHex(TIME_STATUS_REQUEST);
    EXPECT_FALSE(decodeManagementResponse(request.data(), request.size()).has_value());

    const auto reply = fromHex(TIME_STATUS_REPLY);
    EXPECT_FALSE(decodeManagementResponse(reply.data(), 60).has_value());
    EXPECT_FALSE(decodeTimeStatus(std::vector<uint8_t>(10, 0)).has_value());
    EXPECT_FALSE(decodePortState(std::vector<uint8_t>(5, 0)).has_value());
}

TEST(PtpManagement, RejectsErrorReplies) {
    auto reply = fromHex(TIME_STATUS_REPLY);
    reply[48] = 0x00;
    reply[49] = 0x02; // MANAGEMENT_ERROR_STATUS instead of MANAGEMENT
    EXPECT_FALSE(decodeManagementResponse(reply.data(), reply.size()).has_value());
}

} // namespace

} // namespace creatures::timesync

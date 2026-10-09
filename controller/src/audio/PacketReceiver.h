#pragma once
#include "logging/Logger.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace creatures::audio {
class PacketReceiver {
  public:
    enum class Result { Packet, Timeout, Error };
    virtual ~PacketReceiver() = default;
    virtual bool open(const std::string &group, const std::string &interfaceIp, uint16_t port) = 0;
    // Must return within a bounded interval so engine shutdown can join the receiver.
    virtual Result receive(std::vector<uint8_t> &packet, size_t maximumSize) = 0;
};
using PacketReceiverFactory = std::function<std::unique_ptr<PacketReceiver>()>;
[[nodiscard]] std::unique_ptr<PacketReceiver> createUdpPacketReceiver(std::shared_ptr<Logger> log);
} // namespace creatures::audio

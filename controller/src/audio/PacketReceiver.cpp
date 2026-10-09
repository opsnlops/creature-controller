#include "audio/PacketReceiver.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace creatures::audio {
namespace {
class UdpPacketReceiver final : public PacketReceiver {
  public:
    explicit UdpPacketReceiver(std::shared_ptr<Logger> log) : log_(std::move(log)) {}
    ~UdpPacketReceiver() override {
        if (socket_ >= 0)
            close(socket_);
    }
    bool open(const std::string &group, const std::string &interfaceIp, uint16_t port) override;
    Result receive(std::vector<uint8_t> &packet, size_t maximumSize) override;

  private:
    std::shared_ptr<Logger> log_;
    int socket_{-1};
};
bool UdpPacketReceiver::open(const std::string &group, const std::string &interfaceIp, uint16_t port) {
    const char *protocol = "UDP";
    int &socket = socket_;
    socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket < 0) {
        log_->error("Unable to create {} socket for {}: {}", protocol, group, std::strerror(errno));
        return false;
    }

    int reuseAddress = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress)) < 0) {
        log_->warn("Unable to enable SO_REUSEADDR for {}: {}", group, std::strerror(errno));
    }

    int receiveBufferSize = 256 * 1024;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &receiveBufferSize, sizeof(receiveBufferSize)) < 0) {
        log_->warn("Unable to enlarge the {} receive buffer for {}: {}", protocol, group, std::strerror(errno));
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(group.c_str());
    address.sin_port = htons(port);
    if (bind(socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        log_->error("Unable to bind {} socket to {}:{}: {}", protocol, group, port, std::strerror(errno));
        close(socket);
        socket = -1;
        return false;
    }

    ip_mreq membership{};
    membership.imr_multiaddr.s_addr = inet_addr(group.c_str());
    membership.imr_interface.s_addr = inet_addr(interfaceIp.c_str());
    if (setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership, sizeof(membership)) < 0) {
        log_->error("Unable to join {} multicast group {} on {}: {}", protocol, group, interfaceIp,
                    std::strerror(errno));
        close(socket);
        socket = -1;
        return false;
    }

    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0 || fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
        log_->error("Unable to make {} socket non-blocking for {}: {}", protocol, group, std::strerror(errno));
        close(socket);
        socket = -1;
        return false;
    }

    log_->info("Joined {} multicast group {} on {}:{}", protocol, group, interfaceIp, port);
    return true;
}

PacketReceiver::Result UdpPacketReceiver::receive(std::vector<uint8_t> &packet, size_t maximumSize) {
    const int socket = socket_;
    pollfd descriptor{socket, POLLIN, 0};
    // Readiness wakes immediately on arrivals; the timeout only bounds shutdown.
    const int result = poll(&descriptor, 1, 20);
    if (result < 0)
        return errno == EINTR ? Result::Timeout : Result::Error;
    if (result == 0)
        return Result::Timeout;
    if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
        return Result::Error;
    if (!(descriptor.revents & POLLIN))
        return Result::Timeout;

    packet.resize(maximumSize + 1U);
    const ssize_t bytesReceived = recv(socket, packet.data(), packet.size(), 0);
    if (bytesReceived < 0)
        return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? Result::Timeout : Result::Error;
    if (bytesReceived == 0 || static_cast<size_t>(bytesReceived) > maximumSize)
        return Result::Timeout;
    packet.resize(static_cast<size_t>(bytesReceived));
    return Result::Packet;
}

} // namespace
std::unique_ptr<PacketReceiver> createUdpPacketReceiver(std::shared_ptr<Logger> log) {
    return std::make_unique<UdpPacketReceiver>(std::move(log));
}
} // namespace creatures::audio

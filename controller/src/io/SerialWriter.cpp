#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <unistd.h>

#include "creature/logging/Logger.h"
#include "creature/thread/ThreadName.h"
#include "io/Message.h"
#include "io/SerialWriter.h"

namespace creatures::io {

using creatures::io::Message;

SerialWriter::SerialWriter(const std::shared_ptr<Logger> &logger, std::string deviceNode,
                           UARTDevice::module_name moduleName, int fileDescriptor,
                           const std::shared_ptr<MessageQueue<Message>> &outgoingQueue,
                           std::chrono::milliseconds writeTimeout)
    : logger(logger), outgoingQueue(outgoingQueue), deviceNode(std::move(deviceNode)), moduleName(moduleName),
      fileDescriptor(fileDescriptor), writeTimeout(writeTimeout) {

    this->logger->info("creating a new SerialWriter for device {} 🐰", this->deviceNode);
}

void SerialWriter::start() {
    this->logger->info("starting the writer thread");
    creatures::StoppableThread::start();
}

void SerialWriter::run() {
    this->logger->info("hello from the writer thread for {} 📝", this->deviceNode);

    this->threadName = fmt::format("SerialWriter::run for {}", this->deviceNode);
    setThreadName(threadName);

    // Report dropped messages periodically so a slow link doesn't flood the log
    constexpr auto dropReportInterval = std::chrono::seconds(5);
    auto lastDropReport = std::chrono::steady_clock::now();
    u64 reportedDrops = 0;

    while (!stop_requested.load()) {
        // Use timeout-based pop to allow shutdown checking
        auto messageOpt = outgoingQueue->pop_timeout(std::chrono::milliseconds(100));

        // Check if we got a message or if shutdown was requested
        if (!messageOpt.has_value()) {
            if (outgoingQueue->is_shutdown_requested() || stop_requested.load()) {
                break; // Exit gracefully during shutdown
            }
            continue; // Timeout, check again
        }

        Message outgoingMessage = messageOpt.value();

        const auto now = std::chrono::steady_clock::now();
        if (now - lastDropReport >= dropReportInterval) {
            const u64 drops = outgoingQueue->droppedCount();
            if (drops != reportedDrops) {
                this->logger->warn("Serial port {} is falling behind: dropped {} queued messages", this->deviceNode,
                                   drops - reportedDrops);
                reportedDrops = drops;
            }
            lastDropReport = now;
        }

        // Skip empty messages that might come from shutdown
        if (outgoingMessage.payload.empty()) {
            continue;
        }
        this->logger->trace("message to write to module {} on {}: {}",
                            UARTDevice::moduleNameToString(outgoingMessage.module), deviceNode,
                            outgoingMessage.payload);

        // Append a newline character to the message
        outgoingMessage.payload += '\n';

        if (!writeFully(outgoingMessage.payload)) {
            break;
        }

        this->logger->trace("Written {} bytes to module {} on {}", outgoingMessage.payload.length(),
                            UARTDevice::moduleNameToString(outgoingMessage.module), deviceNode);
    }

    if (failed.load()) {
        this->logger->error("SerialWriter for {} stopped because the port failed", this->deviceNode);
    } else {
        this->logger->info("SerialWriter for {} shutting down normally", this->deviceNode);
    }
}

bool SerialWriter::writeFully(const std::string &payload) {
    using namespace std::chrono;

    const auto deadline = steady_clock::now() + writeTimeout;
    std::size_t written = 0;

    while (written < payload.length()) {
        if (stop_requested.load()) {
            return false;
        }

        const ssize_t result = write(this->fileDescriptor, payload.data() + written, payload.length() - written);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
            continue;
        }

        if (result < 0 && errno == EINTR) {
            continue;
        }

        if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            markFailed(fmt::format("Serial port {} write error: {}", this->deviceNode, strerror(errno)));
            return false;
        }

        // The buffer is full. Wait for room in short slices so a shutdown isn't held up.
        const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now());
        if (remaining <= milliseconds(0)) {
            markFailed(fmt::format("Serial port {} stalled: wrote {} of {} bytes in {} ms", this->deviceNode, written,
                                   payload.length(), writeTimeout.count()));
            return false;
        }

        struct pollfd fds[1];
        fds[0].fd = this->fileDescriptor;
        fds[0].events = POLLOUT;
        fds[0].revents = 0;

        const int ret = poll(fds, 1, static_cast<int>(std::min<milliseconds::rep>(remaining.count(), 100)));
        if (ret < 0 && errno != EINTR) {
            markFailed(fmt::format("Serial port {} poll error: {}", this->deviceNode, strerror(errno)));
            return false;
        }

        if (ret > 0 && (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))) {
            markFailed(fmt::format("Serial port {} error detected (revents: {:#x}) - communication lost!",
                                   this->deviceNode, fds[0].revents));
            return false;
        }
    }

    return true;
}

void SerialWriter::markFailed(const std::string &errorMessage) {
    if (stop_requested.load()) {
        return;
    }
    this->logger->error(errorMessage);
    failed.store(true);
}

} // namespace creatures::io
//
// SerialReader.cpp
//

#include <poll.h>
#include <unistd.h>

#include "config/UARTDevice.h"
#include "io/Message.h"
#include "io/SerialReader.h"
#include "util/thread_name.h"

namespace creatures :: io {

    using creatures::config::UARTDevice;
    using creatures::io::Message;

    SerialReader::SerialReader(const std::shared_ptr<Logger>& logger,
                 std::string deviceNode,
                 UARTDevice::module_name moduleName,
                 int fileDescriptor,
                 const std::shared_ptr<MessageQueue<Message>>& incomingQueue) :
                 logger(logger), incomingQueue(incomingQueue), deviceNode(deviceNode),
                 moduleName(moduleName), fileDescriptor(fileDescriptor) {

        this->logger->info("creating a new SerialReader for module {} on {} 🐰",
                           UARTDevice::moduleNameToString(moduleName), deviceNode);
    }

    void SerialReader::start() {
        this->logger->info("starting the reader thread");
        creatures::StoppableThread::start();
    }

    void SerialReader::run() {
        this->logger->info("hello from the reader thread for {} 👓", this->deviceNode);

        this->threadName = fmt::format("SerialReader::{}", this->deviceNode);
        setThreadName(threadName);

        struct pollfd fds[1];
        int timeout_msecs = 100; // Short timeout for responsiveness to shutdown requests

        fds[0].fd = this->fileDescriptor;
        fds[0].events = POLLIN;

        std::string tempBuffer; // Temporary buffer to store incomplete messages

        while(!stop_requested.load()) {
            int ret = poll(fds, 1, timeout_msecs);

            if (ret < 0) {
                // Poll error - this indicates a serious problem, time to hop away! 🐰
                this->logger->error("Poll error on {}: {} - requesting shutdown",
                                   this->deviceNode, strerror(errno));
                break;
            } else if (ret == 0) {
                // Timeout - just continue the loop to check stop_requested again
                // This keeps us responsive like a quick rabbit! 🐰
                continue;
            }

            // Check for error conditions on the file descriptor
            if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                this->logger->error("Serial port {} error detected (revents: 0x{:x}) - requesting shutdown",
                                   this->deviceNode, fds[0].revents);
                break;
            }

            if (fds[0].revents & POLLIN) {
                char readBuf[256];
                memset(&readBuf, '\0', sizeof(readBuf));

                ssize_t numBytes = read(this->fileDescriptor, &readBuf, sizeof(readBuf) - 1);

                if (numBytes < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No data available right now, continue hopping along 🐰
                        continue;
                    } else {
                        // Real read error - time to bail out gracefully
                        this->logger->error("Read error on {}: {} - requesting shutdown",
                                           this->deviceNode, strerror(errno));
                        break;
                    }
                } else if (numBytes == 0) {
                    // EOF - device was disconnected, no point trying to recover
                    this->logger->error("Serial port {} disconnected (EOF) - requesting shutdown",
                                       this->deviceNode);
                    break;
                }

                // Process the incoming data - hop through the buffer! 🐰
                tempBuffer.append(readBuf, numBytes);

                size_t newlinePos;
                while ((newlinePos = tempBuffer.find('\n')) != std::string::npos) {
                    // Extract the complete message
                    std::string messagePayload = tempBuffer.substr(0, newlinePos);

                    // Remove any trailing carriage return (in case of CRLF line endings)
                    if (!messagePayload.empty() && messagePayload.back() == '\r') {
                        messagePayload.pop_back();
                    }

                    // Only process non-empty messages
                    if (!messagePayload.empty()) {
                        Message incomingMessage = Message(this->moduleName, messagePayload);
                        this->logger->trace("adding message '{}' to the incoming queue",
                                           incomingMessage.payload);
                        this->incomingQueue->push(incomingMessage);
                    }

                    // Remove the processed message from tempBuffer
                    tempBuffer.erase(0, newlinePos + 1);
                }
            }
        }

        this->logger->info("SerialReader for {} shutting down - hopping away cleanly! 🐰",
                          this->deviceNode);
    }

}
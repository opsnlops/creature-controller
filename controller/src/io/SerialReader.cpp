//
// SerialReader.cpp
//

#include <iostream>
#include <poll.h>
#include <unistd.h>

#include "config/UARTDevice.h"
#include "io/Message.h"
#include "io/SerialReader.h"
#include "util/thread_name.h"

namespace creatures ::io {

using creatures::config::UARTDevice;
using creatures::io::Message;

SerialReader::SerialReader(const std::shared_ptr<Logger> &logger, std::string deviceNode,
                           UARTDevice::module_name moduleName, int fileDescriptor,
                           const std::shared_ptr<MessageQueue<Message>> &incomingQueue)
    : logger(logger), incomingQueue(incomingQueue), deviceNode(deviceNode), moduleName(moduleName),
      fileDescriptor(fileDescriptor) {

    this->logger->info("creating a new SerialReader for module {} on {} 🐰", UARTDevice::moduleNameToString(moduleName),
                       deviceNode);
}

void SerialReader::start() {
    this->logger->info("starting the reader thread");
    creatures::StoppableThread::start();
}

void SerialReader::run() {
    this->logger->info("hello from the reader thread for {} 👓", this->deviceNode);

    this->threadName = fmt::format("SerialReader::run for {}", this->deviceNode);
    setThreadName(threadName);

    struct pollfd fds[1];
    int timeout_msecs = 21000; // 21 seconds in milliseconds

    fds[0].fd = this->fileDescriptor;
    fds[0].events = POLLIN;

    std::string tempBuffer; // Temporary buffer to store incomplete messages

    while (!stop_requested.load()) {
        int ret = poll(fds, 1, timeout_msecs);

        if (ret < 0) {
            // Check if we're being shut down before treating as fatal
            if (stop_requested.load()) {
                this->logger->info("SerialReader for {} received shutdown during poll", this->deviceNode);
                break;
            }
            // Poll error - log and exit thread gracefully
            std::string errorMessage = fmt::format("Serial port {} poll error: {}", this->deviceNode, strerror(errno));
            this->logger->error(errorMessage);
            break; // Exit thread gracefully instead of calling std::exit
        } else if (ret == 0) {
            this->logger->debug("Poll timeout on {} - no data received", this->deviceNode);
            continue;
        }

        // Check for error conditions on the file descriptor
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            // Check if we're being shut down
            if (stop_requested.load()) {
                this->logger->info("SerialReader for {} received shutdown during error condition", this->deviceNode);
                break;
            }
            // Serial port error detected - log and exit thread gracefully
            std::string errorMessage =
                fmt::format("Serial port {} error detected (revents: {:#x}) - communication lost!", this->deviceNode,
                            fds[0].revents);
            this->logger->error(errorMessage);
            break; // Exit thread gracefully instead of calling std::exit
        }

        if (fds[0].revents & POLLIN) {
            char readBuf[256];
            memset(&readBuf, '\0', sizeof(readBuf));

            ssize_t numBytes = read(this->fileDescriptor, &readBuf, sizeof(readBuf) - 1);

            if (numBytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // This is expected for non-blocking reads - just continue
                    continue;
                } else {
                    // Check if we're being shut down
                    if (stop_requested.load()) {
                        this->logger->info("SerialReader for {} received shutdown during read error", this->deviceNode);
                        break;
                    }
                    // Read error - log and exit thread gracefully
                    std::string errorMessage =
                        fmt::format("Serial port {} read error: {}", this->deviceNode, strerror(errno));
                    this->logger->error(errorMessage);
                    break; // Exit thread gracefully instead of calling std::exit
                }
            } else if (numBytes == 0) {
                // Check if we're being shut down
                if (stop_requested.load()) {
                    this->logger->info("SerialReader for {} received shutdown during EOF", this->deviceNode);
                    break;
                }
                // End of file - this usually means the device was disconnected
                std::string errorMessage =
                    fmt::format("Serial port {} disconnected (EOF) - device unplugged?", this->deviceNode);
                this->logger->warn(errorMessage);
                break; // Exit thread gracefully instead of calling std::exit
            }

            tempBuffer.append(readBuf, numBytes); // Append new data to tempBuffer

            size_t newlinePos;
            while ((newlinePos = tempBuffer.find('\n')) != std::string::npos) {
                // Create the message and SEND IT
                Message incomingMessage = Message(this->moduleName, tempBuffer.substr(0, newlinePos));

                this->logger->trace("adding message '{}' to the incoming queue", incomingMessage.payload);
                this->incomingQueue->push(incomingMessage); // Push the message to the queue
                tempBuffer.erase(0, newlinePos + 1);        // Remove the processed message from tempBuffer
            }
        }
    }

    this->logger->info("SerialReader for {} shutting down normally", this->deviceNode);
}

} // namespace creatures::io
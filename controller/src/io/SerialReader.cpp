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
    int timeout_msecs = 200; // 200 milliseconds

    fds[0].fd = this->fileDescriptor;
    fds[0].events = POLLIN;

    std::string tempBuffer; // Temporary buffer to store incomplete messages

    while (!stop_requested.load()) {
        int ret = poll(fds, 1, timeout_msecs);

        if (ret < 0) {
            if (errno == EINTR) {
                if (stop_requested.load()) {
                    break;
                }
                continue;
            }
            std::string errorMessage = fmt::format("Serial port {} poll error: {}", this->deviceNode, strerror(errno));
            this->logger->error(errorMessage);
            break; // Exit thread gracefully instead of calling std::exit
        } else if (ret == 0) {
            continue;
        }

        // Check for error conditions on the file descriptor
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
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
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    continue;
                }
                std::string errorMessage =
                    fmt::format("Serial port {} read error: {}", this->deviceNode, strerror(errno));
                this->logger->error(errorMessage);
                break; // Exit thread gracefully instead of calling std::exit
            } else if (numBytes == 0) {
                std::string errorMessage =
                    fmt::format("Serial port {} disconnected (EOF) - device unplugged?", this->deviceNode);
                this->logger->warn(errorMessage);
                break; // Exit thread gracefully instead of calling std::exit
            }

            tempBuffer.append(readBuf, numBytes); // Append new data to tempBuffer

            size_t newlinePos;
            while ((newlinePos = tempBuffer.find('\n')) != std::string::npos) {
                std::string line = tempBuffer.substr(0, newlinePos);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (!line.empty()) {
                    Message incomingMessage = Message(this->moduleName, line);
                    this->logger->trace("adding message '{}' to the incoming queue", incomingMessage.payload);
                    this->incomingQueue->push(incomingMessage);
                }

                tempBuffer.erase(0, newlinePos + 1);
            }
        }
    }

    this->logger->info("SerialReader for {} shutting down normally", this->deviceNode);
}

} // namespace creatures::io

#include <iostream>
#include <unistd.h>
#include <poll.h>

#include "controller-config.h"


#include "io/SerialReader.h"
#include "util/thread_name.h"

namespace creatures :: io {

    SerialReader::SerialReader(const std::shared_ptr<Logger>& logger,
                 std::string deviceNode,
                 int fileDescriptor,
                 const std::shared_ptr<MessageQueue<std::string>>& incomingQueue) : logger(logger) {

        this->logger->info("creating a new SerialReader for device {}", deviceNode);
        this->deviceNode = deviceNode;
        this->fileDescriptor = fileDescriptor;
        this->incomingQueue = incomingQueue;

    }

    void SerialReader::start() {
        this->logger->info("starting the reader thread");
        creatures::StoppableThread::start();
    }


    void SerialReader::run() {
        this->logger->info("hello from the reader thread 🔍");

        setThreadName("SerialReader::run");

        struct pollfd fds[1];
        int timeout_msecs = 21000; // 21 seconds in milliseconds

        fds[0].fd = this->fileDescriptor;
        fds[0].events = POLLIN;

        std::string tempBuffer; // Temporary buffer to store incomplete messages

        while(!stop_requested.load()) {
            int ret = poll(fds, 1, timeout_msecs);

            if (ret < 0) {
                this->logger->error("Error on poll: {}", strerror(errno));
                break;
            } else if (ret == 0) {
                this->logger->debug("Poll timeout. No data received.");
                continue;
            }

            if (fds[0].revents & POLLIN) {
                char readBuf[256];
                memset(&readBuf, '\0', sizeof(readBuf));

                size_t numBytes = read(this->fileDescriptor, &readBuf, sizeof(readBuf) - 1); // Leave space for null terminator
                if (numBytes < 0) {
                    this->logger->error("Error reading: {}", strerror(errno));
                    break;
                } else if (numBytes > 0) {
                    tempBuffer.append(readBuf, numBytes); // Append new data to tempBuffer
                    size_t newlinePos;
                    while ((newlinePos = tempBuffer.find('\n')) != std::string::npos) {
                        std::string message = tempBuffer.substr(0, newlinePos);
                        this->logger->trace("adding message '{}' to the incoming queue", message);
                        this->incomingQueue->push(message); // Push the message to the queue
                        tempBuffer.erase(0, newlinePos + 1); // Remove the processed message from tempBuffer
                    }
                }
            }
        }
    }

}

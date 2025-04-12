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
                               const std::shared_ptr<MessageQueue<Message>>& incomingQueue,
                               std::atomic<bool>& resources_valid,
                               std::atomic<bool>& port_connected) :
            logger(logger), incomingQueue(incomingQueue), deviceNode(deviceNode),
            moduleName(moduleName), fileDescriptor(fileDescriptor),
            resources_valid(resources_valid), port_connected(port_connected) {

        this->logger->info("creating a new SerialReader for module {} on {}",
                           UARTDevice::moduleNameToString(moduleName), deviceNode);
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
        int timeout_msecs = 100; // Shorter timeout to allow more frequent checking of stop_requested

        std::string tempBuffer; // Temporary buffer to store incomplete messages

        // Counter for consecutive errors
        int errorCount = 0;
        const int MAX_CONSECUTIVE_ERRORS = 3; // After this many errors, mark device as disconnected

        while(!stop_requested.load()) {
            // Check if resources are still valid before proceeding
            if (!resources_valid.load()) {
                // Resources are no longer valid - sleep briefly and recheck
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Check if the file descriptor is valid
            if (this->fileDescriptor < 0) {
                this->logger->warn("Invalid file descriptor in SerialReader");
                // Mark the port as disconnected
                port_connected.store(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Set up the poll structure
            fds[0].fd = this->fileDescriptor;
            fds[0].events = POLLIN;
            fds[0].revents = 0;

            // Poll with timeout
            int ret = poll(fds, 1, timeout_msecs);

            // Check for poll errors
            if (ret < 0) {
                // Poll system call failed
                errorCount++;
                if (!stop_requested.load() && resources_valid.load()) {
                    this->logger->error("Error on poll: {} (errorCount: {})", strerror(errno), errorCount);
                }

                // If we've had too many consecutive errors, mark the port as disconnected
                if (errorCount >= MAX_CONSECUTIVE_ERRORS) {
                    this->logger->error("Too many consecutive poll errors, marking port as disconnected");
                    port_connected.store(false);
                    // Reset error count after taking action
                    errorCount = 0;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else if (ret == 0) {
                // Poll timeout, just continue the loop
                // Reset error count on successful operations
                errorCount = 0;
                continue;
            }

            // Check if we have data to read
            if (fds[0].revents & POLLIN) {
                // Verify resources are still valid before reading
                if (!resources_valid.load() || stop_requested.load()) {
                    break;
                }

                // Reset error count on successful operations
                errorCount = 0;

                char readBuf[256];
                memset(&readBuf, '\0', sizeof(readBuf));

                // Safe read - check if we can still read
                if (this->fileDescriptor >= 0) {
                    // Read data from the serial port
                    ssize_t numBytes = read(this->fileDescriptor, &readBuf, sizeof(readBuf) - 1); // Leave space for null terminator

                    // Check for read errors
                    if (numBytes < 0) {
                        errorCount++;
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            // This is a real error, not just "no data available"
                            if (!stop_requested.load() && resources_valid.load()) {
                                this->logger->error("Error reading from serial port: {} (errno: {}, errorCount: {})",
                                                    strerror(errno), errno, errorCount);

                                // Special handling for common disconnect errors
                                if (errno == ENXIO || errno == ENODEV || errno == ENOENT || errno == ENOTTY ||
                                    errno == EBADF || errno == EINVAL || errno == EIO || errno == 6) { // Device not configured
                                    this->logger->error("Device error detected, marking port as disconnected");
                                    port_connected.store(false);
                                    // Reset error count after taking action
                                    errorCount = 0;
                                }
                            }
                        }

                        // If we've had too many consecutive errors, mark the port as disconnected
                        if (errorCount >= MAX_CONSECUTIVE_ERRORS) {
                            this->logger->error("Too many consecutive read errors, marking port as disconnected");
                            port_connected.store(false);
                            // Reset error count after taking action
                            errorCount = 0;
                        }

                        // Sleep briefly to avoid busy-waiting
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }

                    // We got 0 bytes - this can happen with non-blocking reads
                    if (numBytes == 0) {
                        // No data available, just continue
                        continue;
                    }

                    // We have valid data to process
                    // Safety check to ensure numBytes is positive and not too large
                    if (numBytes > 0 && numBytes < static_cast<ssize_t>(sizeof(readBuf))) {
                        tempBuffer.append(readBuf, static_cast<size_t>(numBytes)); // Append new data to tempBuffer

                        // Process complete messages (ending with newline)
                        size_t newlinePos;
                        while ((newlinePos = tempBuffer.find('\n')) != std::string::npos) {
                            if (!resources_valid.load() || stop_requested.load()) {
                                break;
                            }

                            // Check if we still have a valid incomingQueue
                            if (this->incomingQueue) {
                                // Create the message and send it
                                Message incomingMessage = Message(this->moduleName, tempBuffer.substr(0, newlinePos));

                                this->logger->trace("adding message '{}' to the incoming queue", incomingMessage.payload);
                                this->incomingQueue->push(incomingMessage); // Push the message to the queue
                            }

                            // Remove the processed message from tempBuffer
                            // Check if newlinePos+1 is within bounds to avoid potential issues
                            if (newlinePos + 1 <= tempBuffer.length()) {
                                tempBuffer.erase(0, newlinePos + 1);
                            } else {
                                tempBuffer.clear();
                            }
                        }
                    } else {
                        // Log unexpected read size
                        this->logger->warn("Unexpected read size: {}", numBytes);
                    }
                }
            } else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                // Handle poll errors
                errorCount++;

                if (!stop_requested.load() && resources_valid.load()) {
                    this->logger->error("Poll error on serial port: {} (errorCount: {})",
                                        (fds[0].revents & POLLERR) ? "POLLERR" :
                                        (fds[0].revents & POLLHUP) ? "POLLHUP" : "POLLNVAL",
                                        errorCount);

                    // POLLNVAL specifically indicates the fd is not open, so mark as disconnected immediately
                    if (fds[0].revents & POLLNVAL) {
                        this->logger->error("POLLNVAL detected, marking port as disconnected");
                        port_connected.store(false);
                        // Reset error count after taking action
                        errorCount = 0;
                    }
                }

                // If we've had too many consecutive errors, mark the port as disconnected
                if (errorCount >= MAX_CONSECUTIVE_ERRORS) {
                    this->logger->error("Too many consecutive poll errors, marking port as disconnected");
                    port_connected.store(false);
                    // Reset error count after taking action
                    errorCount = 0;
                }

                // Sleep to avoid busy-waiting and give time for the issue to resolve
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Clear the buffer on error to avoid processing partial data
                tempBuffer.clear();
            }
        }

        this->logger->info("SerialReader thread for {} stopping", this->deviceNode);
    }
}
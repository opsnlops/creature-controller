#include <unistd.h>

#include "logging/Logger.h"
#include "io/Message.h"
#include "io/SerialWriter.h"
#include "util/thread_name.h"

// If we need to halt, we need this
extern std::atomic<bool> shutdown_requested;

namespace creatures :: io {

    using creatures::io::Message;

    SerialWriter::SerialWriter(const std::shared_ptr<Logger>& logger,
                               std::string deviceNode,
                               UARTDevice::module_name moduleName,
                               int fileDescriptor,
                               const std::shared_ptr<MessageQueue<Message>>& outgoingQueue,
                               std::atomic<bool>& resources_valid,
                               std::atomic<bool>& port_connected) :
            logger(logger),
            outgoingQueue(outgoingQueue),
            deviceNode(std::move(deviceNode)),
            moduleName(moduleName),
            fileDescriptor(fileDescriptor),
            resources_valid(resources_valid),
            port_connected(port_connected) {

        this->logger->info("creating a new SerialWriter for device {}", deviceNode);
    }

    void SerialWriter::start() {
        this->logger->info("starting the writer thread");
        creatures::StoppableThread::start();
    }


    void SerialWriter::run() {
        this->threadName = fmt::format("SerialWriter::run for {}", this->deviceNode);
        setThreadName(threadName);

        this->logger->info("hello from the writer thread for {} 📝", this->deviceNode);

        // Counter for consecutive errors
        int errorCount = 0;
        const int MAX_CONSECUTIVE_ERRORS = 3; // After this many errors, mark device as disconnected

        while(!stop_requested.load()) {
            // Check if resources are still valid
            if (!resources_valid.load()) {
                // Resources no longer valid, sleep briefly and check again
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Check if we have a valid outgoing queue before trying to pop from it
            if (!this->outgoingQueue) {
                this->logger->error("Outgoing queue is null in SerialWriter");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            try {
                // Pop with a timeout to avoid blocking indefinitely
                Message outgoingMessage = outgoingQueue->popWithTimeout(std::chrono::milliseconds(100));

                // Reset error count on successful queue operation
                errorCount = 0;

                // Check if the file descriptor is valid
                if (this->fileDescriptor < 0) {
                    this->logger->warn("Invalid file descriptor in SerialWriter");
                    // Mark the port as disconnected
                    port_connected.store(false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                this->logger->trace("message to write to module {} on {}: {}",
                                    UARTDevice::moduleNameToString(outgoingMessage.module),
                                    deviceNode,
                                    outgoingMessage.payload);

                // Append a newline character to the message
                outgoingMessage.payload += '\n';

                // Verify resources are still valid before writing
                if (!resources_valid.load() || stop_requested.load()) {
                    continue;
                }

                // Safe write - check if we can still write
                if (this->fileDescriptor >= 0) {
                    ssize_t bytesWritten = write(this->fileDescriptor, outgoingMessage.payload.c_str(), outgoingMessage.payload.length());

                    // Check for write errors
                    if (bytesWritten < 0) {
                        errorCount++;

                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            if (!stop_requested.load() && resources_valid.load()) {
                                this->logger->error("Error writing to serial port: {} (errno: {}, errorCount: {})",
                                                    strerror(errno), errno, errorCount);

                                // Special handling for common disconnect errors
                                if (errno == ENXIO || errno == ENODEV || errno == ENOENT || errno == ENOTTY ||
                                    errno == EBADF || errno == EINVAL || errno == EIO || errno == 6) { // Device not configured
                                    this->logger->error("Device error detected, halting.");
                                    port_connected.store(false);
                                    shutdown_requested.store(true);
                                    return;
                                }
                            }
                        }

                        // If we've had too many consecutive errors, mark the port as disconnected
                        if (errorCount >= MAX_CONSECUTIVE_ERRORS) {
                            this->logger->error("Too many consecutive write errors, halting");
                            port_connected.store(false);
                            shutdown_requested.store(true);
                            return;
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }

                    // Reset error count on successful write
                    errorCount = 0;

                    if (bytesWritten > 0) {
                        this->logger->trace("Written {} bytes to module {} on {}",
                                            bytesWritten,
                                            UARTDevice::moduleNameToString(outgoingMessage.module),
                                            deviceNode);
                    }
                }
            } catch (const std::exception& e) {
                // Timeout or other error - check if we should continue
                if (stop_requested.load() || !resources_valid.load()) {
                    continue;
                }

                // Most likely a timeout, which is normal - not an error condition
                // Just loop back and try again
                continue;
            }
        }

        this->logger->info("SerialWriter thread for {} stopping", this->deviceNode);
    }

}
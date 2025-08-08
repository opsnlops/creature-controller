#include <iostream>
#include <unistd.h>

#include "io/Message.h"
#include "io/SerialWriter.h"
#include "logging/Logger.h"
#include "util/thread_name.h"

namespace creatures ::io {

using creatures::io::Message;

SerialWriter::SerialWriter(const std::shared_ptr<Logger> &logger, std::string deviceNode,
                           UARTDevice::module_name moduleName, int fileDescriptor,
                           const std::shared_ptr<MessageQueue<Message>> &outgoingQueue)
    : logger(logger), outgoingQueue(outgoingQueue), deviceNode(std::move(deviceNode)), moduleName(moduleName),
      fileDescriptor(fileDescriptor) {

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

        // Skip empty messages that might come from shutdown
        if (outgoingMessage.payload.empty()) {
            continue;
        }
        this->logger->trace("message to write to module {} on {}: {}",
                            UARTDevice::moduleNameToString(outgoingMessage.module), deviceNode,
                            outgoingMessage.payload);

        // Append a newline character to the message
        outgoingMessage.payload += '\n';

        ssize_t bytesWritten =
            write(this->fileDescriptor, outgoingMessage.payload.c_str(), outgoingMessage.payload.length());

        if (bytesWritten < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This could happen with non-blocking writes - retry once
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                bytesWritten =
                    write(this->fileDescriptor, outgoingMessage.payload.c_str(), outgoingMessage.payload.length());

                if (bytesWritten < 0) {
                    // Check if we're being shut down
                    if (stop_requested.load()) {
                        this->logger->info("SerialWriter for {} received shutdown during write retry",
                                           this->deviceNode);
                        break;
                    }
                    // Still failed - log and exit thread gracefully
                    std::string errorMessage =
                        fmt::format("Serial port {} write error: {}", this->deviceNode, strerror(errno));
                    this->logger->error(errorMessage);
                    break; // Exit thread gracefully instead of calling std::exit
                }
            } else {
                // Check if we're being shut down
                if (stop_requested.load()) {
                    this->logger->info("SerialWriter for {} received shutdown during write error", this->deviceNode);
                    break;
                }
                // Write error - log and exit thread gracefully
                std::string errorMessage =
                    fmt::format("Serial port {} write error: {}", this->deviceNode, strerror(errno));
                this->logger->error(errorMessage);
                break; // Exit thread gracefully instead of calling std::exit
            }
        } else if (bytesWritten != static_cast<ssize_t>(outgoingMessage.payload.length())) {
            // Check if we're being shut down
            if (stop_requested.load()) {
                this->logger->info("SerialWriter for {} received shutdown during partial write", this->deviceNode);
                break;
            }
            // Partial write - log and exit thread gracefully
            std::string errorMessage = fmt::format("Serial port {} partial write - expected {} bytes, wrote {} bytes",
                                                   this->deviceNode, outgoingMessage.payload.length(), bytesWritten);
            this->logger->warn(errorMessage);
            break; // Exit thread gracefully instead of calling std::exit
        }

        this->logger->trace("Written {} bytes to module {} on {}", bytesWritten,
                            UARTDevice::moduleNameToString(outgoingMessage.module), deviceNode);
    }

    this->logger->info("SerialWriter for {} shutting down normally", this->deviceNode);
}

} // namespace creatures::io
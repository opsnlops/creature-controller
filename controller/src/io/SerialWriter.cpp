#include <iostream>
#include <unistd.h>

#include "logging/Logger.h"
#include "io/Message.h"
#include "io/SerialWriter.h"
#include "util/thread_name.h"

namespace creatures :: io {

    using creatures::io::Message;

    SerialWriter::SerialWriter(const std::shared_ptr<Logger>& logger,
                               std::string deviceNode,
                               UARTDevice::module_name moduleName,
                               int fileDescriptor,
                               const std::shared_ptr<MessageQueue<Message>>& outgoingQueue) :
            logger(logger),
            outgoingQueue(outgoingQueue),
            deviceNode(std::move(deviceNode)),
            moduleName(moduleName),
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

        while(!stop_requested.load()) {
            Message outgoingMessage = outgoingQueue->pop();
            this->logger->trace("message to write to module {} on {}: {}",
                                UARTDevice::moduleNameToString(outgoingMessage.module),
                                deviceNode,
                                outgoingMessage.payload);

            // Append a newline character to the message
            outgoingMessage.payload += '\n';

            ssize_t bytesWritten = write(this->fileDescriptor, outgoingMessage.payload.c_str(), outgoingMessage.payload.length());

            if (bytesWritten < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // This could happen with non-blocking writes - retry once
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    bytesWritten = write(this->fileDescriptor, outgoingMessage.payload.c_str(), outgoingMessage.payload.length());

                    if (bytesWritten < 0) {
                        // Still failed - this is fatal
                        std::string errorMessage = fmt::format("FATAL: Serial port {} write error: {}",
                                                               this->deviceNode, strerror(errno));
                        this->logger->critical(errorMessage);
                        std::cerr << errorMessage << " - Cannot write to serial port! Exiting immediately! 🐰💥" << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                } else {
                    // Write error is fatal
                    std::string errorMessage = fmt::format("FATAL: Serial port {} write error: {}",
                                                           this->deviceNode, strerror(errno));
                    this->logger->critical(errorMessage);
                    std::cerr << errorMessage << " - Cannot write to serial port! Exiting immediately! 🐰💥" << std::endl;
                    std::exit(EXIT_FAILURE);
                }
            } else if (bytesWritten != static_cast<ssize_t>(outgoingMessage.payload.length())) {
                // Partial write - this is also problematic for reliable communication
                std::string errorMessage = fmt::format("FATAL: Serial port {} partial write - expected {} bytes, wrote {} bytes",
                                                       this->deviceNode, outgoingMessage.payload.length(), bytesWritten);
                this->logger->critical(errorMessage);
                std::cerr << errorMessage << " - Unreliable serial communication! Exiting immediately! 🐰💥" << std::endl;
                std::exit(EXIT_FAILURE);
            }

            this->logger->trace("Written {} bytes to module {} on {}",
                                bytesWritten,
                                UARTDevice::moduleNameToString(outgoingMessage.module),
                                deviceNode);
        }

        this->logger->info("SerialWriter for {} shutting down normally", this->deviceNode);
    }

}
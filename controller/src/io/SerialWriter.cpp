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
        this->threadName = fmt::format("SerialWriter::{}", this->deviceNode);
        setThreadName(threadName);

        this->logger->info("hello from the writer thread for {} 📝", this->deviceNode);

        while(!stop_requested.load()) {
            // Try to get a message with a short timeout so we stay responsive to shutdown
            auto messageOpt = outgoingQueue->pop_timeout(std::chrono::milliseconds(100));

            if (!messageOpt.has_value()) {
                // Timeout - just continue to check stop_requested
                continue;
            }

            Message outgoingMessage = messageOpt.value();

            // Check if we're shutting down after getting the message
            if (stop_requested.load()) {
                break;
            }

            this->logger->trace("message to write to module {} on {}: {}",
                                UARTDevice::moduleNameToString(outgoingMessage.module),
                                deviceNode,
                                outgoingMessage.payload);

            // Append a newline character to the message
            std::string messageToSend = outgoingMessage.payload + '\n';

            // Write the message to the serial port
            ssize_t bytesWritten = write(this->fileDescriptor, messageToSend.c_str(), messageToSend.length());

            if (bytesWritten < 0) {
                // Write error - time to hop away! 🐰
                this->logger->error("Write error on {}: {} - shutting down",
                                   this->deviceNode, strerror(errno));
                break;
            }

            if (static_cast<size_t>(bytesWritten) != messageToSend.length()) {
                // Partial write - also an error condition
                this->logger->error("Partial write on {} (wrote {}/{} bytes) - shutting down",
                                   this->deviceNode, bytesWritten, messageToSend.length());
                break;
            }

            this->logger->trace("Written {} bytes to module {} on {}",
                                bytesWritten,
                                UARTDevice::moduleNameToString(outgoingMessage.module),
                                deviceNode);
        }

        this->logger->info("SerialWriter for {} shutting down - hopped away cleanly! 🐰", this->deviceNode);
    }

}
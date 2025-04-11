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
                               const std::shared_ptr<MessageQueue<Message>>& outgoingQueue) :  logger(logger),
                                                                                               outgoingQueue(outgoingQueue),
                                                                                               deviceNode(std::move(deviceNode)),
                                                                                               moduleName(moduleName),
                                                                                               fileDescriptor(fileDescriptor) {

        this->logger->info("creating a new SerialWriter for device {}", deviceNode);

    }

    void SerialWriter::start() {
        this->logger->info("starting the reader thread");
        creatures::StoppableThread::start();
    }


    void SerialWriter::run() {
        // Create a local copy of the logger to avoid dangling pointer issues
        auto localLogger = this->logger;

        this->threadName = fmt::format("SerialWriter::run for {}", this->deviceNode);
        setThreadName(threadName);

        localLogger->info("hello from the writer thread for {} 📝", this->deviceNode);

        while(!stop_requested.load()) {
            Message outgoingMessage = outgoingQueue->pop();
            localLogger->trace("message to write to module {} on {}: {}",
                               UARTDevice::moduleNameToString(outgoingMessage.module),
                               deviceNode,
                               outgoingMessage.payload);

            // Append a newline character to the message
            outgoingMessage.payload += '\n';

            size_t bytesWritten = write(this->fileDescriptor, outgoingMessage.payload.c_str(), outgoingMessage.payload.length());

            localLogger->trace("Written {} bytes to module {} on {}",
                               bytesWritten,
                               UARTDevice::moduleNameToString(outgoingMessage.module),
                               deviceNode);
        }
    }

}
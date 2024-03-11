

#include <filesystem>
#include <iostream>
#include <unistd.h>   // UNIX standard function definitions
#include <fcntl.h>    // File control definitions
#include <termios.h>  // POSIX terminal control definitions
#include <poll.h>

#include "controller-config.h"


#include "io/SerialWriter.h"
#include "util/thread_name.h"

namespace creatures :: io {


    SerialWriter::SerialWriter(const std::shared_ptr<Logger>& logger,
                 std::string deviceNode,
                 int fileDescriptor,
                 const std::shared_ptr<MessageQueue<std::string>>& outgoingQueue) : logger(logger) {

        this->logger->info("creating a new SerialWriter for device {}", deviceNode);
        this->deviceNode = deviceNode;
        this->fileDescriptor = fileDescriptor;
        this->outgoingQueue = outgoingQueue;

    }

    void SerialWriter::start() {
        this->logger->info("starting the reader thread");
        creatures::StoppableThread::start();
    }


    void SerialWriter::run() {
        setThreadName("SerialHandler::writer");

        this->logger->info("hello from the writer thread ✏️");

        std::string outgoingMessage;
        while(!stop_requested.load()) {
            outgoingMessage = outgoingQueue->pop();
            this->logger->trace("message to write to {}: {}", deviceNode, outgoingMessage);

            // Append a newline character to the message
            outgoingMessage += '\n';

            size_t bytesWritten = write(this->fileDescriptor, outgoingMessage.c_str(), outgoingMessage.length());

            if (bytesWritten < 0) {
                // Handle write error
                this->logger->error("Error writing to {}: {}", deviceNode, strerror(errno));
                // Consider adding error handling like retry mechanism or breaking the loop
            } else {
                this->logger->trace("Written {} bytes to {}", bytesWritten, deviceNode);
            }
        }
    }

}

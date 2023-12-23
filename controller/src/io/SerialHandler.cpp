

#include <filesystem>
#include <iostream>
#include <unistd.h>   // UNIX standard function definitions
#include <fcntl.h>    // File control definitions
#include <termios.h>  // POSIX terminal control definitions

#include "controller-config.h"

#include "util/thread_name.h"
#include "SerialHandler.h"
#include "SerialException.h"

namespace creatures {

    /**
         * Creates a new SerialHandler
         *
         * @param deviceNode the device node to open up
         * @param outgoingQueue A `MessageQueue<std::string>` for outgoing messages TO the remote device
         * @param incomingQueue A `MessageQueue<std::string>` for incoming messages FROM the remote device
         */
    SerialHandler::SerialHandler(const std::shared_ptr<Logger>& logger,
                                 std::string deviceNode,
                                 const std::shared_ptr<MessageQueue<std::string>> &outgoingQueue,
                                 const std::shared_ptr<MessageQueue<std::string>> &incomingQueue) {

        this->logger = logger;

        this->logger->info("creating a new SerialHandler for device {}", deviceNode);

        // Do some error checking
        if (!outgoingQueue) {
            this->logger->critical("outgoingQueue is null");
            throw SerialException("outgoingQueue is null");
        }

        if (!incomingQueue) {
            this->logger->critical("incomingQueue is null");
            throw SerialException("incomingQueue is null");
        }

        isDeviceNodeAccessible(logger, deviceNode);

        this->outgoingQueue = outgoingQueue;
        this->incomingQueue = incomingQueue;

        this->deviceNode = deviceNode;
        this->fileDescriptor = -1;

        this->logger->debug("new SerialHandler created");
    }


    // Access to our queues
    std::shared_ptr<MessageQueue<std::string>> SerialHandler::getOutgoingQueue() {
        return this->outgoingQueue;
    }
    std::shared_ptr<MessageQueue<std::string>> SerialHandler::getIncomingQueue() {
        return this->incomingQueue;
    }

    bool SerialHandler::setupSerialPort() {

        this->logger->info("attempting to open {}", this->deviceNode);
        this->fileDescriptor = open(this->deviceNode.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
        this->logger->debug("serial point is open, fileDescriptor = {}", this->fileDescriptor);

        if (this->fileDescriptor == -1) {

            // Handle error - unable to open serial port
            this->logger->critical("Error opening {}: {}", this->deviceNode.c_str(), strerror(errno));
            std::exit(1);

        } else {
            struct termios tty;
            if (tcgetattr(this->fileDescriptor, &tty) != 0) {

                // Handle error in tcgetattr
                this->logger->error("Error from tcgetattr: {}", strerror(errno));
            }

            // 8 bits per byte (most common)
            tty.c_cflag &= ~PARENB; // No parity bit
            tty.c_cflag &= ~CSTOPB; // Only one stop bit
            tty.c_cflag &= ~CSIZE;  // Clear all the size bits
            tty.c_cflag |= CS8;     // 8 bits per byte

            tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
            tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

            tty.c_lflag &= ~ICANON;
            tty.c_lflag &= ~ECHO;    // Disable echo
            tty.c_lflag &= ~ECHOE;   // Disable erasure
            tty.c_lflag &= ~ECHONL;  // Disable new-line echo
            tty.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT and SUSP
            tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off software flow control
            tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                             ICRNL); // Disable any special handling of received bytes

            tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g., newline chars)
            tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

            // VMIN and VTIME are used to control block timing. Here we set VMIN to 0 and VTIME to 1,
            // making read non-blocking. The read function will return immediately if there is nothing to read.
            tty.c_cc[VMIN] = 0;
            tty.c_cc[VTIME] = 1;

            cfsetispeed(&tty, B115200);      // Set in baud rate
            cfsetospeed(&tty, B115200);      // Set out baud rate

            if (tcsetattr(this->fileDescriptor, TCSANOW, &tty) != 0) {
                this->logger->error("Error from tcsetattr: {}", strerror(errno));
                return false;
            }

            this->logger->debug("serial port {} is open", this->deviceNode);
        }

        return true;
    }

    void SerialHandler::start() {

        this->logger->info("starting SerialHandler for device {}", deviceNode);

        if (!setupSerialPort()) {
            this->logger->critical("unable to setupSerialPort");
            throw SerialException("unable to setupSerialPort");
        }
        this->logger->debug("setupSerialPort done");

        this->writerThread = std::thread(&SerialHandler::writer, this);
        this->readerThread = std::thread(&SerialHandler::reader, this);

        this->writerThread.detach();
        this->readerThread.detach();

        this->logger->debug("done starting SerialHandler for device {}", deviceNode);
    }

    [[noreturn]] void SerialHandler::writer() {
        setThreadName("SerialHandler::writer");

        this->logger->info("hello from the writer thread");

        std::string outgoingMessage;
        for (EVER) {
            outgoingMessage = outgoingQueue->pop();
            this->logger->trace("message to write to {}: {}", deviceNode, outgoingMessage);

            // Append a newline character to the message
            outgoingMessage += '\n';

            ssize_t bytesWritten = write(this->fileDescriptor, outgoingMessage.c_str(), outgoingMessage.length());

            if (bytesWritten < 0) {
                // Handle write error
                this->logger->error("Error writing to {}: {}", deviceNode, strerror(errno));
                // Consider adding error handling like retry mechanism or breaking the loop
            } else {
                this->logger->debug("Written {} bytes to {}", bytesWritten, deviceNode);
            }
        }
    }

    void SerialHandler::reader() {
        this->logger->info("hello from the reader thread 🔍");

        setThreadName("SerialHandler::reader");

        fd_set read_fds;
        struct timeval timeout{};
        timeout.tv_sec = 21;  // 21 seconds (Stats are sent every 20!)
        timeout.tv_usec = 0;

        std::string tempBuffer; // Temporary buffer to store incomplete messages

        for (EVER) {
            FD_ZERO(&read_fds);
            FD_SET(this->fileDescriptor, &read_fds);

            int selectStatus = select(this->fileDescriptor + 1, &read_fds, nullptr, nullptr, &timeout);

            if (selectStatus < 0) {
                this->logger->error("Error on select: {}", strerror(errno));
                break;
            } else if (selectStatus == 0) {
                this->logger->debug("Select timeout. No data received.");
                continue;
            }

            if (FD_ISSET(this->fileDescriptor, &read_fds)) {
                char readBuf[256];
                memset(&readBuf, '\0', sizeof(readBuf));

                int numBytes = read(this->fileDescriptor, &readBuf, sizeof(readBuf) - 1); // Leave space for null terminator
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

    /**
     * Makes sure that a device node exists, and is a character device
     *
     * @param node the file system node to check
     * @return true if all is well
     * @throws SerialException if it's not
     */
    bool SerialHandler::isDeviceNodeAccessible(std::shared_ptr<Logger> _logger, const std::string &node) {
        namespace fs = std::filesystem;

        // Check if the file exists
        if (!fs::exists(node)) {
            std::string errorMessage = fmt::format("Device node does not exist: {}", node);
            _logger->critical(errorMessage);
            throw SerialException(errorMessage);
        }

        // Check if it's a character device
        if (!fs::is_character_file(fs::status(node))) {
            std::string errorMessage = fmt::format("Device node is not a character device: {}", node);
            _logger->critical(errorMessage);
            throw SerialException(errorMessage);
        }

        return true;
    }


} // creatures

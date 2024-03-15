
#include <filesystem>

#include <fcntl.h>
#include <termios.h>


#include "controller-config.h"
#include "config/UARTDevice.h"
#include "io/SerialHandler.h"
#include "io/SerialReader.h"
#include "io/SerialWriter.h"
#include "io/SerialException.h"

namespace creatures {

    using creatures::config::UARTDevice;
    using creatures::io::Message;

    /**
     * Creates a new SerialHandler
     *
     * @param deviceNode the device node to open up
     * @param outgoingQueue A `MessageQueue<Message>` for outgoing messages TO the remote device
     * @param incomingQueue A `MessageQueue<Message>` for incoming messages FROM the remote device
     */
    SerialHandler::SerialHandler(const std::shared_ptr<Logger>& logger,
                                 std::string deviceNode,
                                 UARTDevice::module_name moduleName,
                                 const std::shared_ptr<MessageQueue<Message>> &outgoingQueue,
                                 const std::shared_ptr<MessageQueue<Message>> &incomingQueue) :
                                 deviceNode(deviceNode), moduleName(moduleName), logger(logger) {

        this->logger->info("creating a new SerialHandler for device {} on node {} ",
                           UARTDevice::moduleNameToString(moduleName), deviceNode);

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
        this->fileDescriptor = -1;

        this->logger->debug("new SerialHandler created");
    }


    UARTDevice::module_name SerialHandler::getModuleName() {
        return this->moduleName;
    }

    // Access to our queues
    std::shared_ptr<MessageQueue<Message>> SerialHandler::getOutgoingQueue() {
        return this->outgoingQueue;
    }
    std::shared_ptr<MessageQueue<Message>> SerialHandler::getIncomingQueue() {
        return this->incomingQueue;
    }

    bool SerialHandler::setupSerialPort() {

        this->logger->info("attempting to open {}", this->deviceNode);
        this->fileDescriptor = open(this->deviceNode.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
        this->logger->debug("serial point is open, fileDescriptor = {}", this->fileDescriptor);

        if (this->fileDescriptor == -1) {

            // Handle error - unable to open serial port
            std::string errorMessage = fmt::format("Error opening {}: {}", this->deviceNode.c_str(), strerror(errno));
            this->logger->critical(errorMessage);
            throw SerialException(errorMessage);

        } else {
            struct termios tty{};
            if (tcgetattr(this->fileDescriptor, &tty) != 0) {

                // Handle error in tcgetattr
                std::string errorMessage = fmt::format("Error from tcgetattr while opening the port: {}", strerror(errno));
                this->logger->critical(errorMessage);
                throw SerialException(errorMessage);
            }

            // 8 bits per byte
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

            cfsetispeed(&tty, BAUD_RATE);      // Set in baud rate
            cfsetospeed(&tty, BAUD_RATE);      // Set out baud rate

            if (tcsetattr(this->fileDescriptor, TCSANOW, &tty) != 0) {
                std::string errorMessage = fmt::format("Error from tcsetattr while configuring the port: {}", strerror(errno));
                this->logger->critical(errorMessage);
                throw SerialException(errorMessage);
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

        std::shared_ptr<creatures::StoppableThread> reader = std::make_shared<creatures::io::SerialReader>(this->logger,
                                                                                                           this->deviceNode,
                                                                                                           this->moduleName,
                                                                                                           this->fileDescriptor,
                                                                                                           this->incomingQueue);
        reader->start();

        std::shared_ptr<creatures::StoppableThread> writer = std::make_shared<creatures::io::SerialWriter>(this->logger,
                                                                                                           this->deviceNode,
                                                                                                           this->moduleName,
                                                                                                           this->fileDescriptor,
                                                                                                           this->outgoingQueue);
        writer->start();

        // Store the workers
        this->threads.push_back(reader);
        this->threads.push_back(writer);

        this->logger->debug("done starting SerialHandler for device {}", deviceNode);

    }


    /**
     * Makes sure that a device node exists, and is a character device
     *
     * @param node the file system node to check
     * @return true if all is well
     * @throws SerialException if it's not
     */
    bool SerialHandler::isDeviceNodeAccessible(const std::shared_ptr<Logger>& _logger, const std::string &node) {
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

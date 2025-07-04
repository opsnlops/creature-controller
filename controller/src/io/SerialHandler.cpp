//
// SerialHandler.cpp
//

#include <filesystem>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "controller-config.h"
#include "config/UARTDevice.h"
#include "io/SerialHandler.h"
#include "io/SerialReader.h"
#include "io/SerialWriter.h"
#include "io/SerialException.h"
#include "util/Result.h"

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

        this->logger->info("creating a new SerialHandler for device {} on node {} 🐰",
                           UARTDevice::moduleNameToString(moduleName), deviceNode);

        // Basic error checking - if these fail, we can't operate!
        if (!outgoingQueue) {
            std::string errorMessage = "FATAL: outgoingQueue is null - cannot operate without message queues!";
            this->logger->critical(errorMessage);
            std::cerr << errorMessage << " - Exiting immediately! 🐰💥" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        if (!incomingQueue) {
            std::string errorMessage = "FATAL: incomingQueue is null - cannot operate without message queues!";
            this->logger->critical(errorMessage);
            std::cerr << errorMessage << " - Exiting immediately! 🐰💥" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        // Check if the device node is accessible - if not, we're done!
        if (!isDeviceNodeAccessible(logger, deviceNode)) {
            std::string errorMessage = fmt::format("FATAL: Device node {} is not accessible", deviceNode);
            this->logger->critical(errorMessage);
            std::cerr << errorMessage << " - Exiting immediately! 🐰💥" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        this->outgoingQueue = outgoingQueue;
        this->incomingQueue = incomingQueue;
        this->fileDescriptor = -1;

        this->logger->debug("SerialHandler created successfully for device {} on node {}",
                            UARTDevice::moduleNameToString(moduleName), deviceNode);
    }

    // Clean up the serial port
    SerialHandler::~SerialHandler() {
        shutdown();
        this->logger->debug("SerialHandler destroyed");
    }

    UARTDevice::module_name SerialHandler::getModuleName() const {
        return this->moduleName;
    }

    // Access to our queues
    std::shared_ptr<MessageQueue<Message>> SerialHandler::getOutgoingQueue() {
        return this->outgoingQueue;
    }

    std::shared_ptr<MessageQueue<Message>> SerialHandler::getIncomingQueue() {
        return this->incomingQueue;
    }

    Result<bool> SerialHandler::setupSerialPort() {
        this->logger->info("attempting to open {}", this->deviceNode);
        this->fileDescriptor = open(this->deviceNode.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
        this->logger->debug("serial port is open, fileDescriptor = {}", this->fileDescriptor);

        if (this->fileDescriptor == -1) {
            // Serial port failed to open - this is fatal for a control system!
            std::string errorMessage = fmt::format("FATAL: Cannot open serial port {}: {}",
                                                   this->deviceNode.c_str(), strerror(errno));
            this->logger->critical(errorMessage);
            std::cerr << errorMessage << " - Exiting immediately! 🐰💥" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        struct termios tty{};
        if (tcgetattr(this->fileDescriptor, &tty) != 0) {
            // Can't configure the port - also fatal!
            std::string errorMessage = fmt::format("FATAL: Error configuring serial port {}: {}",
                                                   this->deviceNode, strerror(errno));
            this->logger->critical(errorMessage);
            std::cerr << errorMessage << " - Exiting immediately!" << std::endl;
            close(this->fileDescriptor);
            std::exit(EXIT_FAILURE);
        }

        // Configure the port (same settings as before)
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

        tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes
        tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 1;

        cfsetispeed(&tty, BAUD_RATE);
        cfsetospeed(&tty, BAUD_RATE);

        if (tcsetattr(this->fileDescriptor, TCSANOW, &tty) != 0) {
            // Can't apply the settings - fatal!
            std::string errorMessage = fmt::format("FATAL: Error applying settings to serial port {}: {}",
                                                   this->deviceNode, strerror(errno));
            this->logger->critical(errorMessage);
            std::cerr << errorMessage << " - Exiting immediately! 🐰💥" << std::endl;
            close(this->fileDescriptor);
            std::exit(EXIT_FAILURE);
        }

        this->logger->debug("serial port {} is open and configured successfully", this->deviceNode);
        return Result<bool>{true};
    }

    Result<bool> SerialHandler::closeSerialPort() {
        if(this->fileDescriptor != -1) {
            this->logger->info("closing {}", this->deviceNode);
            close(this->fileDescriptor);
            this->fileDescriptor = -1;
        }
        return Result<bool>{true};
    }

    Result<bool> SerialHandler::start() {
        this->logger->info("starting SerialHandler for device {}", deviceNode);

        // Try to set up the serial port - if this fails, we exit immediately
        setupSerialPort();
        this->logger->debug("setupSerialPort done");

        // Create the reader and writer threads
        reader = std::make_shared<creatures::io::SerialReader>(this->logger,
                                                               this->deviceNode,
                                                               this->moduleName,
                                                               this->fileDescriptor,
                                                               this->incomingQueue);

        writer = std::make_shared<creatures::io::SerialWriter>(this->logger,
                                                               this->deviceNode,
                                                               this->moduleName,
                                                               this->fileDescriptor,
                                                               this->outgoingQueue);

        // Start both threads
        reader->start();
        writer->start();

        this->logger->debug("SerialHandler for {} is hopping along nicely! 🐰", deviceNode);
        return Result<bool>{true};
    }

    Result<bool> SerialHandler::shutdown() {
        this->logger->info("shutting down SerialHandler for device {}", deviceNode);

        // First, stop the reader thread if it exists
        if (reader) {
            this->logger->debug("stopping reader thread");
            reader->shutdown();
        }

        // Then stop the writer thread if it exists
        if (writer) {
            this->logger->debug("stopping writer thread");
            writer->shutdown();
        }

        // Finally, close the serial port
        closeSerialPort();
        return Result<bool>{true};
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
            _logger->critical("Device node does not exist: {}", node);
            return false;
        }

        // Check if it's a character device
        if (!fs::is_character_file(fs::status(node))) {
            _logger->critical("Device node is not a character device: {}", node);
            return false;
        }

        return true;
    }

    void SerialHandler::shutdownThreads() {
        this->logger->info("shutting down SerialHandler threads for device {}", deviceNode);

        // Stop the reader thread if it exists
        if (reader) {
            this->logger->debug("stopping reader thread {}", reader->getName());
            reader->shutdown();
        }

        // Stop the writer thread if it exists
        if (writer) {
            this->logger->debug("stopping writer thread {}", writer->getName());
            writer->shutdown();
        }

        this->logger->debug("all SerialHandler threads shutdown requested for device {}", deviceNode);
    }

} // creatures
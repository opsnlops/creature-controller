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
            deviceNode(deviceNode), moduleName(moduleName), logger(logger),
            resources_valid(true), port_connected(false) {

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

        try {
            // Just check if the device node is accessible - don't open it yet
            isDeviceNodeAccessible(logger, deviceNode);
        } catch (const SerialException& e) {
            // Log the error but don't propagate the exception - we'll handle reconnection later
            this->logger->warn("Device node not initially accessible: {}", e.what());
        }

        this->outgoingQueue = outgoingQueue;
        this->incomingQueue = incomingQueue;
        this->fileDescriptor = -1;

        this->logger->debug("new SerialHandler created");
    }

    // Clean up the serial port
    SerialHandler::~SerialHandler() {
        // Make sure we're fully shut down before destruction
        shutdown();
        this->logger->debug("SerialHandler destroyed");
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

    bool SerialHandler::areResourcesValid() const {
        return resources_valid.load();
    }

    bool SerialHandler::isPortConnected() const {
        return port_connected.load() && fileDescriptor >= 0;
    }

    Result<bool> SerialHandler::setupSerialPort() {
        // Don't set up if resources are already invalidated
        if (!resources_valid.load()) {
            return Result<bool>{ControllerError(ControllerError::IOError, "Resources already invalidated")};
        }

        // If the file descriptor is already open, close it first
        if (this->fileDescriptor >= 0) {
            closeSerialPort();
        }

        this->logger->info("attempting to open {}", this->deviceNode);

        // Check if the device exists before trying to open it
        try {
            isDeviceNodeAccessible(logger, deviceNode);
        } catch (const SerialException& e) {
            this->logger->error("Cannot access device node: {}", e.what());
            port_connected.store(false);
            return Result<bool>{ControllerError(ControllerError::IOError, e.what())};
        }

        this->fileDescriptor = open(this->deviceNode.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
        this->logger->debug("serial port open attempt, fileDescriptor = {}", this->fileDescriptor);

        if (this->fileDescriptor == -1) {
            // Handle error - unable to open serial port
            std::string errorMessage = fmt::format("Error opening {}: {}", this->deviceNode.c_str(), strerror(errno));
            this->logger->critical(errorMessage);
            port_connected.store(false);
            return Result<bool>{ControllerError(ControllerError::IOError, errorMessage)};
        } else {
            struct termios tty{};
            if (tcgetattr(this->fileDescriptor, &tty) != 0) {
                // Handle error in tcgetattr
                std::string errorMessage = fmt::format("Error from tcgetattr while opening the port: {}", strerror(errno));
                this->logger->critical(errorMessage);
                close(this->fileDescriptor);
                this->fileDescriptor = -1;
                port_connected.store(false);
                return Result<bool>{ControllerError(ControllerError::IOError, errorMessage)};
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
                close(this->fileDescriptor);
                this->fileDescriptor = -1;
                port_connected.store(false);
                return Result<bool>{ControllerError(ControllerError::IOError, errorMessage)};
            }

            this->logger->debug("serial port {} is open", this->deviceNode);
            port_connected.store(true);
        }

        return Result<bool>{true};
    }

    Result<bool> SerialHandler::closeSerialPort() {
        if(this->fileDescriptor != -1) {
            this->logger->info("closing {}", this->deviceNode);
            close(this->fileDescriptor);
            this->fileDescriptor = -1;
            port_connected.store(false);
        }
        return Result<bool>{true};
    }

    Result<bool> SerialHandler::start() {
        // Don't start if we're already shutting down
        if (!resources_valid.load()) {
            return Result<bool>{ControllerError(ControllerError::IOError, "Cannot start, resources already invalidated")};
        }

        this->logger->info("starting SerialHandler for device {}", deviceNode);

        auto setupResult = setupSerialPort();
        if (!setupResult.isSuccess()) {
            auto errorMessage = fmt::format("unable to setupSerialPort: {}", setupResult.getError()->getMessage());
            this->logger->critical(errorMessage);
            return Result<bool>{ControllerError(ControllerError::IOError, errorMessage)};
        }
        this->logger->debug("setupSerialPort done");

        // Clear any previous threads
        threads.clear();

        // Create reader and writer threads with port_connected reference
        std::shared_ptr<creatures::StoppableThread> reader = std::make_shared<creatures::io::SerialReader>(
                this->logger,
                this->deviceNode,
                this->moduleName,
                this->fileDescriptor,
                this->incomingQueue,
                this->resources_valid,
                this->port_connected
        );

        std::shared_ptr<creatures::StoppableThread> writer = std::make_shared<creatures::io::SerialWriter>(
                this->logger,
                this->deviceNode,
                this->moduleName,
                this->fileDescriptor,
                this->outgoingQueue,
                this->resources_valid,
                this->port_connected
        );

        // Start both threads
        reader->start();
        writer->start();

        // Store the workers
        this->threads.push_back(reader);
        this->threads.push_back(writer);

        this->logger->debug("done starting SerialHandler for device {}", deviceNode);

        return Result<bool>{true};
    }

    Result<bool> SerialHandler::reconnect() {
        this->logger->info("attempting to reconnect to {}", deviceNode);

        // First shutdown existing connections
        auto shutdownResult = shutdown();
        if (!shutdownResult.isSuccess()) {
            auto errorMessage = fmt::format("Failed to shut down before reconnect: {}",
                                            shutdownResult.getError()->getMessage());
            this->logger->warn(errorMessage);
            // Continue anyway - we'll try to reconnect
        }

        // Reset our internal state
        resources_valid.store(true);
        port_connected.store(false);

        // Clear any pending messages in the queues
        if (this->outgoingQueue) {
            this->outgoingQueue->clear();
        }
        if (this->incomingQueue) {
            this->incomingQueue->clear();
        }

        // Small delay to ensure the port is fully released
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        // Try to reopen and restart
        return start();
    }

    Result<bool> SerialHandler::shutdown() {
        this->logger->info("shutting down SerialHandler for device {}", deviceNode);

        // First, mark resources as invalid to signal threads to stop using them
        resources_valid.store(false);
        port_connected.store(false);

        // Stop all threads
        for (auto& thread : threads) {
            if (thread) {
                this->logger->debug("stopping thread {}", thread->getName());
                thread->shutdown();
            }
        }

        // Wait for threads to actually stop (with a timeout)
        for (auto& thread : threads) {
            if (thread) {
                // Give each thread up to 500ms to stop
                for (int i = 0; i < 50; i++) {
                    if (!thread->isThreadJoinable()) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                if (thread->isThreadJoinable()) {
                    this->logger->warn("Thread {} did not stop in time", thread->getName());
                }
            }
        }

        // Clear threads vector
        threads.clear();

        // Close the port after threads are stopped
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
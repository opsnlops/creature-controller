

#include <filesystem>
#include <iostream>
#include <unistd.h>   // UNIX standard function definitions
#include <fcntl.h>    // File control definitions
#include <termios.h>  // POSIX terminal control definitions

#include "controller-config.h"
#include "namespace-stuffs.h"



#include "SerialOutput.h"
#include "SerialException.h"

namespace creatures {

    /**
         * Creates a new SerialOutput
         *
         * @param deviceNode the device node to open up
         * @param outgoingQueue A `MessageQueue<std::string>` for outgoing messages TO the remote device
         * @param incomingQueue A `MessageQueue<std::string>` for incoming messages FROM the remote device
         */
    SerialOutput::SerialOutput(std::string deviceNode,
                               const std::shared_ptr<MessageQueue<std::string>>& outgoingQueue,
                               const std::shared_ptr<MessageQueue<std::string>>& incomingQueue) {

        info("creating a new SerialOutput for device {}", deviceNode);

        // Do some error checking
        if( !outgoingQueue ) {
            critical("outgoingQueue is null");
            throw SerialException("outgoingQueue is null");
        }

        if( !incomingQueue ) {
            critical("incomingQueue is null");
            throw SerialException("incomingQueue is null");
        }

        isDeviceNodeAccessible(deviceNode);

        this->outgoingQueue = outgoingQueue;
        this->incomingQueue = incomingQueue;

        this->deviceNode = deviceNode;
        this->fileDescriptor = -1;

        debug("new SerialOutput created");
    }


    bool SerialOutput::setupSerialPort() {

        info("attempting to open {}", this->deviceNode);
        this->fileDescriptor = open(this->deviceNode.c_str(), O_RDWR | O_NOCTTY);
        debug("serial point is open, fileDescriptor = {}", this->fileDescriptor);

        if (this->fileDescriptor == -1) {

            // Handle error - unable to open serial port
            critical("Error opening {}: {}", this->deviceNode.c_str(), strerror(errno));
            std::exit(1);

        } else {
            struct termios tty;
            if (tcgetattr(this->fileDescriptor, &tty) != 0) {

                // Handle error in tcgetattr
                error("Error from tcgetattr: {}", strerror(errno));
            }

            tty.c_cflag &= ~PARENB;        // No parity bit
            tty.c_cflag &= ~CSTOPB;        // One stop bit
            tty.c_cflag &= ~CSIZE;
            tty.c_cflag |= CS8;            // 8 bits per byte
            tty.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS hardware flow control
            tty.c_cflag |= CREAD | CLOCAL; // Enable reading and ignore ctrl lines

            cfsetispeed(&tty, B9600);      // Set in baud rate
            cfsetospeed(&tty, B9600);      // Set out baud rate

            if (tcsetattr(this->fileDescriptor, TCSANOW, &tty) != 0) {
                error("Error from tcsetattr: {}", strerror(errno));
                return false;
            }

            debug("serial port {} is open", this->deviceNode);
        }

        return true;
    }

    void SerialOutput::start() {

        info("starting SerialOutput for device {}", deviceNode);

        if( !setupSerialPort() ) {
            critical("unable to setupSerialPort");
            throw SerialException("unable to setupSerialPort");
        }
        debug("setupSerialPort done");

        this-> writerThread = std::thread(&SerialOutput::writer, this);
        this->readerThread = std::thread(&SerialOutput::reader, this);

        this->writerThread.detach();
        this->readerThread.detach();

        debug("done starting SerialOutput for device {}", deviceNode);
    }

    [[noreturn]] void SerialOutput::writer() {

        info("hello from the writer thread 🔍");

        std::string outgoingMessage;
        for(EVER) {
            outgoingMessage = outgoingQueue->pop();
            debug("message to write to {}: {}", deviceNode, outgoingMessage);
        }
    }

    void SerialOutput::reader() {

        info("hello from the reader thread ✏️");

        // TODO: implement me

    }

    /**
     * Makes sure that a device node exists, and is a character device
     *
     * @param node the file system node to check
     * @return true if all is well
     * @throws SerialException if it's not
     */
    bool SerialOutput::isDeviceNodeAccessible(const std::string& node) {
        namespace fs = std::filesystem;

        // Check if the file exists
        if (!fs::exists(node)) {
            std::string errorMessage = fmt::format("Device node does not exist: {}", node);
            critical(errorMessage);
            throw SerialException(errorMessage);
        }

        // Check if it's a character device
        if (!fs::is_character_file(fs::status(node))) {
            std::string errorMessage = fmt::format("Device node is not a character device: {}", node);
            critical(errorMessage);
            throw SerialException(errorMessage);
        }

        return true;
    }


} // creatures

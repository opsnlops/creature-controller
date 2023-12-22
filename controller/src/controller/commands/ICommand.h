
#pragma once

#include <string>

#include "controller-config.h"


namespace creatures {

    /**
     * @brief An interface for commands to the firmware
     */

    class ICommand {
    public:
        virtual ~ICommand() = default;

        /**
         * Convert the command into a message to go on the wire
         * @return
         */
        virtual std::string toMessage() = 0;
        virtual std::string toMessageWithChecksum() = 0;

        /*
         * Get the checksum of this message as a u16
         */
        u16 getChecksum() {
            u16 checksum = 0;

            // Calculate checksum
            for (char c : toMessage()) {
                checksum += c;
            }

           return checksum;
        }
    };

}
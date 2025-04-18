
#pragma once

#include <string>
#include <fmt/format.h>

#include "controller-config.h"


namespace creatures {

    /**
     * @brief An interface for commands to the firmware
     */

    class ICommand {
    public:
        virtual ~ICommand() = default;

        /**
         * @brief Convert the command into a message string without a checksum
         *
         * @return a string representation of the command
         */
        virtual std::string toMessage() = 0;


        /*
         * Get the checksum of this message as a u16
         */
        u16 getChecksum() {
            u16 checksum = 0;

            // Calculate checksum
            for (const char c : toMessage()) {
                checksum += c;
            }

           return checksum;
        }

        /**
         * @brief  Convert this message into one that can be sent on the wire with a check at the end
         *
         * @return the complete message with a checksum
         */
        std::string toMessageWithChecksum() {
            return fmt::format("{}\tCS {}", toMessage(), getChecksum());
        }
    };

}
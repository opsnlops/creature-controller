

#include <string>
#include <vector>

#include "controller-config.h"

#include "logging/Logger.h"
#include "io/handlers/StatsHandler.h"
#include "io/handlers/StatsMessage.h"
#include "util/string_utils.h"


namespace creatures {

    void StatsHandler::handle(std::shared_ptr<Logger> logger, const std::vector<std::string> &tokens) {

        logger->debug("incoming stats!");

        auto statsMessage = StatsMessage();

        for(std::string token : tokens) {

            std::vector<std::string> split = splitString(token);

            // Make sure we have the right number of tokens
            if(split.empty() || split.size() > 2) {
                logger->warn("invalid token in {} message: {}", STATS_MESSAGE, token);
                continue;
            }

            // Split the name and value
            std::string name = split[0];

            // If this is a stats message, there won't be a second piece
            std::string value;
            if(split.size() == 2) {
                value = split[1];
            }

            if(name == STATS_MESSAGE) {} // do nothing

            // Memory
            else if(name == STATS_HEAP_FREE) {
                statsMessage.freeHeap = stringToU64(value);
            }

            // USB
            else if(name == STATS_USB_CHARACTERS_RECEIVED) {
                statsMessage.uSBCharactersReceived = stringToU64(value);
            }
            else if(name == STATS_USB_MESSAGES_RECEIVED) {
                statsMessage.uSBMessagesReceived = stringToU64(value);
            }
            else if(name == STATS_USB_MESSAGES_SENT) {
                statsMessage.uSBMessagesSent = stringToU64(value);
            }


            // UART
            else if(name == STATS_UART_CHARACTERS_RECEIVED) {
                statsMessage.uARTCharactersReceived = stringToU64(value);
            }
            else if(name == STATS_UART_MESSAGES_RECEIVED) {
                statsMessage.uARTMessagesReceived = stringToU64(value);
            }
            else if(name == STATS_UART_MESSAGES_SENT) {
                statsMessage.uARTMessagesSent = stringToU64(value);
            }


            // Message Processor
            else if(name == STATS_MP_MESSAGES_RECEIVED) {
                statsMessage.mPMessagesReceived = stringToU64(value);
            }
            else if(name == STATS_MP_MESSAGES_SENT) {
                statsMessage.mPMessagesSent = stringToU64(value);
            }


            // Parsing
            else if(name == STATS_SUCCESSFUL_PARSE) {
                statsMessage.parseSuccesses = stringToU64(value);
            }
            else if(name == STATS_FAILED_PARSE) {
                statsMessage.parseFailures = stringToU64(value);
            }
            else if(name == STATS_CHECKSUM_FAILED) {
                statsMessage.checksumFailures = stringToU64(value);
            }

            // Movement
            else if(name == STATS_POSITIONS_PROCESSED) {
                statsMessage.positionMessagesProcessed = stringToU64(value);
            }

            // PWM
            else if(name == STATS_PWM_WRAPS) {
                statsMessage.pwmWraps = stringToU64(value);
            }

            // Board sensors
            else if(name == STATS_BOARD_TEMPERATURE) {
                statsMessage.boardTemperature = stringToDouble(value);
            }

            else {
                logger->warn("unknown token in {} message: {}", STATS_MESSAGE, token);
            }
        }

        // Now log it!
        logger->info(statsMessage.toString());
    }

} // creatures
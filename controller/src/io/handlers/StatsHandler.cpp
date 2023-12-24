

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

            else if(name == STATS_HEAP_FREE) {
                statsMessage.freeHeap = stringToU64(value);
            }

            else if(name == STATS_CHARACTERS_RECEIVED) {
                statsMessage.charactersReceived = stringToU64(value);
            }

            else if(name == STATS_MESSAGES_RECEIVED) {
                statsMessage.messagesReceived = stringToU64(value);
            }

            else if(name == STATS_MESSAGES_SENT) {
                statsMessage.messagesSent = stringToU64(value);
            }

            else if(name == STATS_SUCCESSFUL_PARSE) {
                statsMessage.parseSuccesses = stringToU64(value);
            }

            else if(name == STATS_FAILED_PARSE) {
                statsMessage.parseFailures = stringToU64(value);
            }

            else if(name == STATS_CHECKSUM_FAILED) {
                statsMessage.checksumFailures = stringToU64(value);
            }

            else if(name == STATS_POSITIONS_PROCESSED) {
                statsMessage.positionMessagesProcessed = stringToU64(value);
            }

            else if(name == STATS_PWM_WRAPS) {
                statsMessage.pwmWraps = stringToU64(value);
            }

            else {
                logger->warn("unknown token in {} message: {}", STATS_MESSAGE, token);
            }
        }

        // Now log it!
        logger->debug(statsMessage.toString());
    }

} // creatures
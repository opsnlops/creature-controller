
#include "logging/logging.h"
#include "messaging/messaging.h"

#include "controller-config.h"

volatile u64 position_messages_processed = 0UL;

bool handlePositionMessage(const GenericMessage* msg) {

    debug("handling position message");

    for(const char *token : msg->tokens) {
        position_messages_processed = position_messages_processed + 1;
    }



    return true;


}


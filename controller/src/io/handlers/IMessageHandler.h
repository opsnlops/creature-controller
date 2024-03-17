
#pragma once

#include <string>
#include <vector>

#include "logging/Logger.h"

namespace creatures {
    class InitHandler;
    /**
     * Message Handler Interface
     */
    class IMessageHandler {
    public:
        virtual ~IMessageHandler() = default;
        virtual void handle(std::shared_ptr<Logger> logger,const std::vector<std::string> &tokens) = 0;
    };
}
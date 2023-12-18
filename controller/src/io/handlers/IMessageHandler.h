
#pragma once

#include <string>
#include <vector>

namespace creatures {

    /**
     * Message Handler Interface
     */
    class IMessageHandler {
    public:
        virtual ~IMessageHandler() = default;
        virtual void handle(const std::vector<std::string> &tokens) = 0;
    };
}
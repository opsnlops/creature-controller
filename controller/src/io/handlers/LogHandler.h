
#pragma once

#include "io/handlers/IMessageHandler.h"

namespace creatures {

    class LogHandler : public IMessageHandler {
    public:
        void handle(const std::vector<std::string>& tokens) override;
    };

} // creatures


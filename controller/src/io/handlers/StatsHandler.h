
#pragma once

#include "io/handlers/IMessageHandler.h"

namespace creatures {

    class StatsHandler : public IMessageHandler {
    public:
        void handle(const std::vector<std::string>& tokens) override;
    };

} // creatures


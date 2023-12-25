
#include <string>

#include <fmt/format.h>

#include "ServoPosition.h"

namespace creatures {


    ServoPosition::ServoPosition(std::string outputPosition, u32 requestedTicks) :
            outputPosition(std::move(outputPosition)), requestedTicks(requestedTicks) {}

    std::string ServoPosition::getOutputPosition() const {
        return outputPosition;
    }

    u32 ServoPosition::getRequestedTicks() const {
        return requestedTicks;
    }

    std::string ServoPosition::toString() const {
        return fmt::format("{} {}", outputPosition, requestedTicks);
    }

} // creatures
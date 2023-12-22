
#include <string>

#include <fmt/format.h>

#include "ServoPosition.h"

namespace creatures {


    ServoPosition::ServoPosition(std::string outputPosition, u32 requestedPosition) :
    outputPosition(std::move(outputPosition)), requestedPosition(requestedPosition) {}

    std::string ServoPosition::getOutputPosition() const {
        return outputPosition;
    }

    u32 ServoPosition::getRequestedPosition() const {
        return requestedPosition;
    }

    std::string ServoPosition::toString() const {
        return fmt::format("{} {}", outputPosition, requestedPosition);
    }

} // creatures
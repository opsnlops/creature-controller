
#include <string>

#include <fmt/format.h>


#include "device/ServoSpecifier.h"
#include "ServoPosition.h"

namespace creatures {


    ServoPosition::ServoPosition(const ServoSpecifier servoId, const u32 requestedTicks) :
            servoId(servoId), requestedTicks(requestedTicks) {}

    ServoSpecifier ServoPosition::getServoId() const {
        return servoId;
    }

    u32 ServoPosition::getRequestedTicks() const {
        return requestedTicks;
    }

    std::string ServoPosition::toString() const {
        return fmt::format("{} {}", servoId.pin, requestedTicks);
    }

} // creatures
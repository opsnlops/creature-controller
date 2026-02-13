
#include <string>

#include <fmt/format.h>

#include "ServoPosition.h"
#include "creature/MotorType.h"
#include "device/ServoSpecifier.h"

namespace creatures {

ServoPosition::ServoPosition(const ServoSpecifier servoId, const u32 requestedTicks)
    : servoId(servoId), requestedTicks(requestedTicks) {}

ServoSpecifier ServoPosition::getServoId() const { return servoId; }

u32 ServoPosition::getRequestedTicks() const { return requestedTicks; }

std::string ServoPosition::toString() const {
    if (servoId.type == creatures::creature::dynamixel) {
        return fmt::format("D{} {}", servoId.pin, requestedTicks);
    }
    return fmt::format("{} {}", servoId.pin, requestedTicks);
}

} // namespace creatures
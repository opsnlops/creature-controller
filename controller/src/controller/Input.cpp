
#include "Input.h"

#include <fmt/format.h>
#include <utility>

#include "controller-config.h"

namespace creatures {

Input::Input(std::string name, u16 slot, u8 width, u32 incomingRequest)
    : name(std::move(name)), slot(slot), width(width), incomingRequest(incomingRequest) {}

Input::Input(const Input &other)
    : name(other.name), slot(other.slot), width(other.width), incomingRequest(other.incomingRequest) {}

std::string Input::toString() const {
    return fmt::format("[ name: {}, slot: {}, width: {}, incomingRequest: {} ]", name, slot, width, incomingRequest);
}

std::string Input::getName() const { return name; }

u16 Input::getSlot() const { return slot; }

u8 Input::getWidth() const { return width; }

u32 Input::getIncomingRequest() const { return incomingRequest; }

void Input::setIncomingRequest(u32 _incomingRequest) { this->incomingRequest = _incomingRequest; }

} // namespace creatures
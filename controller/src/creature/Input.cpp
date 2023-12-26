
#include "Input.h"

#include <utility>

#include "controller-config.h"

namespace creatures {

    Input::Input(std::string  name, u16 slot, u8 width) :
    name(std::move(name)), slot(slot), width(width) {}

    std::string Input::getName() const {
        return name;
    }

    u16 Input::getSlot() const {
        return slot;
    }

    u8 Input::getWidth() const {
        return width;
    }


} // creatures
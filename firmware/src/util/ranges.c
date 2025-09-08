
#include "logging/logging.h"

#include "ranges.h"
#include "types.h"

i32 convertRange(i32 input, const i32 oldMin, const i32 oldMax, const i32 newMin, const i32 newMax) {

    if( input > oldMax ) {
        const i32 newInput = oldMax;
        warning("input (%d) is out of range %d to %d. capping at %d", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    if( input < oldMin ) {
        const i32 newInput = oldMin;
        warning("input (%d) is out of range %d to %d. capping at %d", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    const i32 oldRange = oldMax - oldMin;
    const i32 newRange = newMax - newMin;
    const i32 newValue = (((input - oldMin) * newRange) / oldRange) + newMin;

    verbose("mapped %d -> %d", input, newValue);
    return newValue;
}

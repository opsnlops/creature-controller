
#include "logging/logging.h"

#include "ranges.h"
#include "types.h"

i32 convertRange(i32 input, const i32 oldMin, const i32 oldMax, const i32 newMin, const i32 newMax) {

    // Clamping an out-of-range input is this helper's normal, designed
    // behavior (it is called every frame from hot paths such as the status
    // lights), so report it at verbose rather than warning.
    if (input > oldMax) {
        const i32 newInput = oldMax;
        verbose("value %d above range %d..%d, clamping to %d", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    if (input < oldMin) {
        const i32 newInput = oldMin;
        verbose("value %d below range %d..%d, clamping to %d", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    const i32 oldRange = oldMax - oldMin;
    const i32 newRange = newMax - newMin;
    const i32 newValue = (((input - oldMin) * newRange) / oldRange) + newMin;

    verbose("mapped %d -> %d", input, newValue);
    return newValue;
}

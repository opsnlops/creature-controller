
#include <stdint.h>


#include "logging/logging.h"

#include "ranges.h"

int32_t convertRange(int32_t input, int32_t oldMin, int32_t oldMax, int32_t newMin, int32_t newMax) {

    if( input > oldMax ) {
        int32_t newInput = oldMax;
        warning("input (%d) is out of range %d to %d. capping at %d", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    if( input < oldMin ) {
        int32_t newInput = oldMin;
        warning("input (%d) is out of range %d to %d. capping at %d", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    int32_t oldRange = oldMax - oldMin;
    int32_t newRange = newMax - newMin;
    int32_t newValue = (((input - oldMin) * newRange) / oldRange) + newMin;

    verbose("mapped %d -> %d", input, newValue);
    return newValue;
}

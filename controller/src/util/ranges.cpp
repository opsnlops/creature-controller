
#include <cstdint>

#include "namespace-stuffs.h"


int32_t convertRange(int32_t input, int32_t oldMin, int32_t oldMax, int32_t newMin, int32_t newMax) {

    if( input > oldMax ) {
        int32_t newInput = oldMax;
        warn("input ({}) is out of range {} to {}. capping at {}", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    if( input < oldMin ) {
        int32_t newInput = oldMin;
        warn("input ({}) is out of range {} to {}. capping at {}", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    int32_t oldRange = oldMax - oldMin;
    int32_t newRange = newMax - newMin;
    int32_t newValue = (((input - oldMin) * newRange) / oldRange) + newMin;

    trace("mapped {} -> {}", input, newValue);
    return newValue;
}
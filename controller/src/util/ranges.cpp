
#include "ranges.h"
#include "logging/Logger.h"

int32_t convertRange(std::shared_ptr<creatures::Logger> logger, int32_t input,
                     int32_t oldMin, int32_t oldMax, int32_t newMin,
                     int32_t newMax) {

    if (input > oldMax) {
        int32_t newInput = oldMax;
        logger->warn("input ({}) is out of range {} to {}. capping at {}",
                     input, oldMin, oldMax, newInput);
        input = newInput;
    }

    if (input < oldMin) {
        int32_t newInput = oldMin;
        logger->warn("input ({}) is out of range {} to {}. capping at {}",
                     input, oldMin, oldMax, newInput);
        input = newInput;
    }

    int32_t oldRange = oldMax - oldMin;
    int32_t newRange = newMax - newMin;
    int32_t newValue = (((input - oldMin) * newRange) / oldRange) + newMin;

    logger->trace("mapped {} -> {}", input, newValue);
    return newValue;
}
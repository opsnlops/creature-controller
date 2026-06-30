
#include "ranges.h"
#include "logging/Logger.h"

int32_t convertRange(std::shared_ptr<creatures::Logger> logger, int32_t input, int32_t oldMin, int32_t oldMax,
                     int32_t newMin, int32_t newMax) {

    // Clamping an out-of-range input is this helper's normal, designed
    // behavior, so report it at debug rather than warning to keep info quiet.
    if (input > oldMax) {
        int32_t newInput = oldMax;
        logger->debug("value {} above range {}..{}, clamping to {}", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    if (input < oldMin) {
        int32_t newInput = oldMin;
        logger->debug("value {} below range {}..{}, clamping to {}", input, oldMin, oldMax, newInput);
        input = newInput;
    }

    int32_t oldRange = oldMax - oldMin;
    int32_t newRange = newMax - newMin;
    int32_t newValue = (((input - oldMin) * newRange) / oldRange) + newMin;

    logger->trace("mapped {} -> {}", input, newValue);
    return newValue;
}

#pragma once

#include <cstdint>

#include "logging/Logger.h"

int32_t convertRange(std::shared_ptr<creatures::Logger> logger, int32_t input,
                     int32_t oldMin, int32_t oldMax, int32_t newMin,
                     int32_t newMax);
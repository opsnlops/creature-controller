
#pragma once

#include <cstdint>
#include <memory>

#include "controller-config.h"
#include "logging/Logger.h"

namespace creatures::creature {

struct head_position_t {
    u16 left;
    u16 right;
};

class DifferentialHead {
  public:
    DifferentialHead(std::shared_ptr<creatures::Logger> logger, float headOffsetMaxPercent, u16 positionMin,
                     u16 positionMax);

    [[nodiscard]] u16 convertToHeadHeight(u16 y) const;
    [[nodiscard]] int32_t convertToHeadTilt(u16 x) const;
    [[nodiscard]] head_position_t calculateHeadPosition(u16 height, int32_t offset) const;

  private:
    std::shared_ptr<creatures::Logger> logger;
    u16 headOffsetMax;
    u16 positionMin;
    u16 positionMax;
};

} // namespace creatures::creature


#include <cmath>

#include "creature/DifferentialHead.h"
#include "util/ranges.h"

namespace creatures::creature {

DifferentialHead::DifferentialHead(std::shared_ptr<creatures::Logger> logger, float headOffsetMaxPercent,
                                   u16 positionMin, u16 positionMax)
    : logger(std::move(logger)), positionMin(positionMin), positionMax(positionMax) {
    headOffsetMax = lround((double)(positionMax - positionMin) * (double)headOffsetMaxPercent);
    this->logger->debug("DifferentialHead: headOffsetMax = {}", headOffsetMax);
}

u16 DifferentialHead::convertToHeadHeight(u16 y) const {
    return convertRange(logger, y, positionMin, positionMax, positionMin + (headOffsetMax / 2),
                        positionMax - (headOffsetMax / 2));
}

int32_t DifferentialHead::convertToHeadTilt(u16 x) const {
    return convertRange(logger, x, positionMin, positionMax, 1 - (headOffsetMax / 2), headOffsetMax / 2);
}

head_position_t DifferentialHead::calculateHeadPosition(u16 height, int32_t offset) const {
    head_position_t headPosition;
    headPosition.left = height - offset;
    headPosition.right = height + offset;

    logger->trace("calculated head position: height: {}, offset: {} -> {}, {}", height, offset, headPosition.right,
                  headPosition.left);

    return headPosition;
}

} // namespace creatures::creature

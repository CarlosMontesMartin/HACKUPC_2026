//===-- common/Types.cpp ----------------------------------------*- C++ -*-===//
// Implementation of free helpers attached to the shared types.
//===----------------------------------------------------------------------===//

#include "Types.hpp"

#include <algorithm>
#include <limits>

namespace warehouse {

void WarehouseData::computeBounds() {
    if (perimeter.empty()) {
        minX = minY = maxX = maxY = 0.0f;
        return;
    }
    minX = maxX = perimeter.front().x;
    minY = maxY = perimeter.front().y;
    for (const auto& v : perimeter) {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
        maxX = std::max(maxX, v.x);
        maxY = std::max(maxY, v.y);
    }
}

float WarehouseData::ceilingAt(float x) const {
    if (ceiling.empty()) {
        return std::numeric_limits<float>::infinity();
    }
    // ceiling is kept sorted by x. Use upper_bound + step back.
    auto it = std::upper_bound(
        ceiling.begin(), ceiling.end(), x,
        [](float v, const CeilingPoint& cp) { return v < cp.x; });
    if (it == ceiling.begin()) {
        return ceiling.front().height; // before first segment - take first
    }
    --it;
    return it->height;
}

} // namespace warehouse

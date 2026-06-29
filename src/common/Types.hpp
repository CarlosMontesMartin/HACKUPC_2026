#pragma once
//===-- common/Types.hpp ----------------------------------------*- C++ -*-===//
// Shared POD-like types that flow between the four modules.
// Kept dependency-free (only STL) so every module can include it.
//===----------------------------------------------------------------------===//

#include <cstdint>
#include <string>
#include <vector>

namespace warehouse {

/// Vertex of the warehouse perimeter (world units, e.g. millimetres).
struct WarehouseVertex {
    float x{0.0f};
    float y{0.0f};
};

/// Axis-aligned obstacle inside the warehouse.
struct Obstacle {
    float x{0.0f};      ///< Bottom-left X
    float y{0.0f};      ///< Bottom-left Y
    float width{0.0f};  ///< Extent along X
    float depth{0.0f};  ///< Extent along Y
};

/// One ceiling segment (piecewise-constant profile along X).
/// `x` marks the start of the segment; the segment continues until the next
/// CeilingPoint's `x` (or +infinity for the last entry).
struct CeilingPoint {
    float x{0.0f};
    float height{0.0f};
};

/// Specification of a bay type.
struct BayType {
    int   id{0};
    float width{0.0f};
    float depth{0.0f};
    float height{0.0f};
    float gap{0.0f};     ///< operational/clearance metadata (informational)
    int   nLoads{0};
    float price{0.0f};
};

/// Output of the optimisation phase: a concrete bay placed in the world.
/// Coordinates refer to the bay's bottom-left corner BEFORE rotation,
/// then rotated about that origin (the rotation rule is documented in
/// optimization/Optimizer.hpp).
struct PlacedBay {
    int   id{0};                ///< BayType id
    float x{0.0f};
    float y{0.0f};
    float rotation{0.0f};       ///< degrees, any value in [0, 360)
    float footprintWidth{0.0f}; ///< AABB width in world axes after rotation
    float footprintDepth{0.0f}; ///< AABB depth in world axes after rotation
    float bayWidth{0.0f};       ///< local-frame (un-rotated) width
    float bayDepth{0.0f};       ///< local-frame (un-rotated) depth
    float height{0.0f};
    float gap{0.0f};            ///< clearance depth in local +Y direction
};

/// Aggregated input data + computed bounds. This is the contract produced
/// by Module 1 (DataInput) and consumed by every later module.
struct WarehouseData {
    std::vector<WarehouseVertex> perimeter;
    std::vector<Obstacle>        obstacles;
    std::vector<CeilingPoint>    ceiling;   ///< sorted by x, ascending
    std::vector<BayType>         bayTypes;

    // Computed AABB of the perimeter polygon.
    float minX{0.0f};
    float minY{0.0f};
    float maxX{0.0f};
    float maxY{0.0f};

    /// Recompute the AABB from `perimeter`.
    void computeBounds();

    /// Ceiling height at world X (piecewise constant). Returns +infinity if
    /// no ceiling data is available (= unconstrained).
    float ceilingAt(float x) const;
};

} // namespace warehouse

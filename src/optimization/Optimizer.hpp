#pragma once
//===-- optimization/Optimizer.hpp ------------------------------*- C++ -*-===//
// Module 2 - Processing.
//
// Multi-strategy greedy heuristic that places bays inside the warehouse
// subject to:
//   * the perimeter polygon (axis-aligned, possibly non-rectangular),
//   * rectangular obstacles,
//   * piecewise-constant ceiling height profile along X,
//   * the per-bay-type GAP clearance constraint (see below),
//   * exhaustive 1-degree rotation search (0..359).
//
// It minimises the cost function
//     Q = ( sum_prices / sum_loads ) ^ ( 2 - %areaUsed )
//
// Multi-strategy search

// The optimizer runs the greedy heuristic multiple times with different
// configurations (ranking functions × sweep directions) and returns the
// result with the lowest Q score.
//
// Ranking modes control which bay types are preferred:
//   - LOADS_AREA_PRICE : nLoads * area / price  (value per cost per area)
//   - LOADS_PRICE      : nLoads / price          (loads per cost)
//   - AREA_PRICE       : area / price            (area per cost)
//   - LOADS2_PRICE     : nLoads² / price          (loads-heavy)
//   - CHEAPEST         : 1 / price               (cheapest first)
//   - BIGGEST          : area                    (biggest first)
//
// Sweep directions control the fill order:
//   - BL_TR : bottom-left → top-right
//   - TR_BL : top-right → bottom-left
//   - TL_BR : top-left → bottom-right
//   - BR_TL : bottom-right → top-left
//
// Rotation search (exhaustive 1-degree steps)

// At every candidate cell we evaluate ALL 360 integer degrees (0..359).
// Sin/cos values are precomputed in a lookup table for performance.
//
// Gap rule (per the PRD ampliation)

// Each bay-type has a `gap` clearance that must be kept empty IN FRONT OF
// the bay (the side where local Y = depth). The gap is a rectangle of
// dimensions (width, gap) attached to the bay's "front" face, rotated
// together with the bay.
//
//   * Hard collision: no SOLID (warehouse wall, obstacle, or another
//     bay's solid structure) may intersect a gap.
//   * Two gaps MAY overlap (front-to-front aisle sharing): the effective
//     distance between two facing bays is max(gap1, gap2), gaps do NOT
//     sum.
//
// Implementation

// The grid is a single uint8_t per cell with three states:
//     0 : free
//     1 : solid  (wall / obstacle / bay solid - nothing else may sit here)
//     2 : gap   (only OTHER gaps may share this cell - no solids)
//
// Solid placement requires every covered cell == 0.
// Gap   placement requires every covered cell != 1 (i.e. 0 or 2).
//
// For arbitrary rotations the bay's solid AABB and the gap rectangle are
// each tested as oriented bounding boxes (OBBs); a cell is "covered" iff
// its centre lies inside the OBB (4 cross-product half-plane tests).
//===----------------------------------------------------------------------===//

#include "../common/Types.hpp"

#include <cstdint>
#include <vector>

namespace warehouse {

class Optimizer {
public:
    /// Aggregated result of one optimisation run.
    struct Result {
        std::vector<PlacedBay> placements;
        double totalPrice         = 0.0;
        long long totalLoads      = 0;
        double usedArea           = 0.0;
        double warehouseArea      = 0.0;
        double percentageAreaUsed = 0.0; ///< in [0, 1]
        double qScore             = 0.0;
    };

    /// Sweep directions for the greedy fill.
    enum class SweepDir { BL_TR, TR_BL, TL_BR, BR_TL };

    /// Ranking modes for bay type ordering.
    enum class RankMode {
        LOADS_AREA_PRICE,   // nLoads * area / price
        LOADS_PRICE,        // nLoads / price
        AREA_PRICE,         // area / price
        LOADS2_PRICE,       // nLoads^2 / price
        CHEAPEST,           // 1 / price
        BIGGEST,            // area
    };

    /// `cellSize` is the resolution (world units) of the internal occupancy
    /// grid. Smaller = more accurate but slower. 100 mm is a good default
    /// for typical warehouse problems expressed in millimetres.
    explicit Optimizer(const WarehouseData& data, float cellSize = 100.0f);

    /// Execute the multi-strategy heuristic. Tries all ranking × sweep
    /// combinations and returns the result with the lowest Q score.
    Result run();

    /// Oriented Bounding Box: 4 corners in world units, ordered CCW.
    /// Public so that file-local helpers in the .cpp can take one by ref.
    struct OBB {
        float cx[4];
        float cy[4];
    };

private:
    // Cell occupancy values.
    static constexpr std::uint8_t kFree  = 0;
    static constexpr std::uint8_t kSolid = 1;
    static constexpr std::uint8_t kGap   = 2;

    /*
     * Internal Helper Methods
     */
    void initBaseGrid();
    bool isInsidePolygon(float x, float y) const;

    // Helper to compile the compute shader
    unsigned int compileComputeShader() const;

    inline int   worldToGridX(float x) const { return static_cast<int>((x - m_data.minX) / m_cellSize); }
    inline int   worldToGridY(float y) const { return static_cast<int>((y - m_data.minY) / m_cellSize); }
    inline float gridToWorldX(int gx)  const { return m_data.minX + gx * m_cellSize; }
    inline float gridToWorldY(int gy)  const { return m_data.minY + gy * m_cellSize; }

    /*
     * Internal State and Configuration
     */
    const WarehouseData& m_data;
    float m_cellSize;
    int   m_gridW = 0;
    int   m_gridH = 0;
    std::vector<std::uint32_t> m_baseGridPacked;
    double m_freeStartArea = 0.0;
};

} // namespace warehouse

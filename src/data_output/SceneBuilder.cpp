//===-- data_output/SceneBuilder.cpp ----------------------------*- C++ -*-===//
// Module 3 implementation. Strictly transforms data, no I/O, no GL.
//===----------------------------------------------------------------------===//

#include "SceneBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace warehouse {

/*
 * Utility helper functions for polygon tests, color generation, and mesh building.
 */

bool SceneBuilder::pointInPolygon(float x, float y,
                                  const std::vector<WarehouseVertex>& poly) {
    bool inside = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const bool yi = (poly[i].y > y);
        const bool yj = (poly[j].y > y);
        if (yi != yj) {
            const float xCross =
                (poly[j].x - poly[i].x) * (y - poly[i].y) /
                (poly[j].y - poly[i].y) + poly[i].x;
            if (x < xCross) inside = !inside;
        }
    }
    return inside;
}

glm::vec3 SceneBuilder::colorForBayId(int id) {
    // Deterministic golden-angle hash to RGB - good visual separation
    // without storing a palette.
    const float golden = 0.61803398875f;
    const float h = std::fmod(static_cast<float>(id) * golden, 1.0f);
    const float s = 0.65f, v = 0.85f;
    // HSV -> RGB.
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    const int seg = static_cast<int>(h * 6.0f) % 6;
    switch (seg) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    return {r + m, g + m, b + m};
}

void SceneBuilder::buildPolygonMesh(const std::vector<WarehouseVertex>& poly,
                                    const std::vector<float>& xs,
                                    const std::vector<float>& ys,
                                    float yLevel,
                                    std::vector<glm::vec3>& tris) {
    if (xs.size() < 2 || ys.size() < 2) return;
    for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
        for (std::size_t j = 0; j + 1 < ys.size(); ++j) {
            const float cx = 0.5f * (xs[i] + xs[i + 1]);
            const float cy = 0.5f * (ys[j] + ys[j + 1]);
            if (!pointInPolygon(cx, cy, poly)) continue;
            const glm::vec3 p00{xs[i],     yLevel, ys[j]    };
            const glm::vec3 p10{xs[i + 1], yLevel, ys[j]    };
            const glm::vec3 p11{xs[i + 1], yLevel, ys[j + 1]};
            const glm::vec3 p01{xs[i],     yLevel, ys[j + 1]};
            // CCW from above (top view).
            tris.push_back(p00); tris.push_back(p10); tris.push_back(p11);
            tris.push_back(p00); tris.push_back(p11); tris.push_back(p01);
        }
    }
}

void SceneBuilder::buildCeilingMesh(const WarehouseData& data,
                                    std::vector<glm::vec3>& tris) {
    /*
     * Generates a triangulated mesh for the warehouse ceiling.
     * The ceiling height can vary along the X-axis (piecewise constant).
     * To ensure the mesh accurately represents both the warehouse perimeter
     * and the height variations, we create a grid using all unique X coordinates
     * from both the perimeter and the ceiling profile, and all unique Y coordinates
     * from the perimeter.
     * We then evaluate the center of each resulting grid cell to see if it falls
     * inside the warehouse polygon. If it does, we generate two triangles for that
     * cell at the appropriate ceiling height.
     */
    if (data.ceiling.empty()) return;

    // X coords: polygon vertices PLUS ceiling segment boundaries -> the
    // mesh respects both the polygon outline and the height steps.
    std::vector<float> xs, ys;
    xs.reserve(data.perimeter.size() + data.ceiling.size());
    ys.reserve(data.perimeter.size());
    for (const auto& v : data.perimeter) {
        xs.push_back(v.x);
        ys.push_back(v.y);
    }
    for (const auto& c : data.ceiling) {
        xs.push_back(c.x);
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

    for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
        for (std::size_t j = 0; j + 1 < ys.size(); ++j) {
            const float cx = 0.5f * (xs[i] + xs[i + 1]);
            const float cy = 0.5f * (ys[j] + ys[j + 1]);
            if (!pointInPolygon(cx, cy, data.perimeter)) continue;
            
            // Retrieve the ceiling height for the center of this cell
            const float h = data.ceilingAt(cx);
            if (!std::isfinite(h)) continue;
            
            const glm::vec3 p00{xs[i],     h, ys[j]    };
            const glm::vec3 p10{xs[i + 1], h, ys[j]    };
            const glm::vec3 p11{xs[i + 1], h, ys[j + 1]};
            const glm::vec3 p01{xs[i],     h, ys[j + 1]};
            
            // Wind so the underside (visible from below) faces -Y.
            tris.push_back(p00); tris.push_back(p11); tris.push_back(p10);
            tris.push_back(p00); tris.push_back(p01); tris.push_back(p11);
        }
    }
}

/*
 * Public API implementation for scene building.
 * Constructs the 3D representation of the warehouse, bays, and gaps.
 */

Scene SceneBuilder::build(const WarehouseData& data,
                          const std::vector<PlacedBay>& placements) {
    /*
     * Main builder function that converts logical warehouse data and optimized
     * bay placements into a renderable 3D Scene object.
     * This involves generating geometry for the floor, ceiling, obstacles,
     * placed bays, and gap visualizations.
     */
    Scene s;

    // World metrics: calculate the center and extents of the warehouse.
    s.worldCenter = {
        0.5f * (data.minX + data.maxX),
        0.0f,
        0.5f * (data.minY + data.maxY)
    };
    s.worldExtents = {
        data.maxX - data.minX,
        1.0f,
        data.maxY - data.minY
    };

    // Floor mesh, clipped to perimeter.
    {
        std::vector<float> xs, ys;
        xs.reserve(data.perimeter.size());
        ys.reserve(data.perimeter.size());
        for (const auto& v : data.perimeter) {
            xs.push_back(v.x);
            ys.push_back(v.y);
        }
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        std::sort(ys.begin(), ys.end());
        ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
        buildPolygonMesh(data.perimeter, xs, ys, 0.0f, s.floorVertices);
    }

    // Perimeter line loop.
    s.perimeterLineLoop.reserve(data.perimeter.size());
    for (const auto& v : data.perimeter) {
        s.perimeterLineLoop.push_back({v.x, 0.5f, v.y});
    }

    // Ceiling mesh (translucent).
    buildCeilingMesh(data, s.ceilingVertices);

    // Obstacles -> boxes. The PRD does not give obstacle heights so
    // a fixed value is used; this only affects visualisation.
    constexpr float kObstacleHeight = 1500.0f;
    s.obstacles.reserve(data.obstacles.size());
    for (const auto& o : data.obstacles) {
        BoxInstance bi;
        bi.position     = {o.x, 0.0f, o.y};
        bi.size         = {o.width, kObstacleHeight, o.depth};
        bi.rotationRadY = 0.0f;
        bi.color        = {0.55f, 0.55f, 0.60f}; // grey-blue
        s.obstacles.push_back(bi);
    }

    // Bays.
    //
    // Coordinate-convention note: the optimiser uses a 2D plane (x, y)
    // where +y is the bay's "depth" direction. In 3D we map that 2D y
    // to world Z (vertical = world Y). The optimiser's CCW rotation
    // rule
    //   (lx, ly) -> (lx*cosθ - ly*sinθ, lx*sinθ + ly*cosθ)
    // does NOT match GLM's right-hand rule for Y-axis rotation, so we
    // pass the renderer the negated angle.
    s.bays.reserve(placements.size());
    for (const auto& p : placements) {
        BoxInstance bi;
        bi.position     = {p.x, 0.0f, p.y};
        bi.size         = {p.bayWidth, p.height, p.bayDepth};
        bi.rotationRadY = -glm::radians(p.rotation);
        bi.color        = colorForBayId(p.id);
        s.bays.push_back(bi);
    }

    // Gaps.
    //
    // Each gap is a thin slab on the floor in front of the bay. The
    // anchor in WORLD coordinates is the bay's local (0, depth) corner
    // rotated by the same angle:
    //   gap_anchor = (px - depth*sinθ, 0, py + depth*cosθ)
    // The slab's local size is (width, gapVizHeight, gap), rotated by
    // the same `rotationRadY` as the bay so the renderer's standard
    // box draw pipeline does the right thing.
    constexpr float kGapVizHeight = 30.0f; // 30mm tall - reads as floor stripe
    s.gaps.reserve(placements.size());
    for (const auto& p : placements) {
        if (p.gap <= 0.0f) continue;
        const float radOpt = glm::radians(p.rotation);
        const float c = std::cos(radOpt);
        const float sR = std::sin(radOpt);
        BoxInstance bi;
        bi.position     = {p.x - p.bayDepth * sR, 0.0f,
                           p.y + p.bayDepth * c};
        bi.size         = {p.bayWidth, kGapVizHeight, p.gap};
        bi.rotationRadY = -glm::radians(p.rotation);
        // BoxInstance.color is vec3; the alpha for gaps is supplied by
        // Scene.gapColor in the renderer. Store the RGB component here
        // so a single uniform path covers solids and gaps.
        bi.color        = {0.85f, 0.0f, 0.95f};
        s.gaps.push_back(bi);
    }

    return s;
}

} // namespace warehouse

#pragma once
//===-- data_output/SceneBuilder.hpp ----------------------------*- C++ -*-===//
// Module 3 - Data Output / Internal Mapping.
//
// Transforms the raw optimisation result + warehouse geometry into a tidy
// `Scene` value object that the renderer (Module 4) consumes directly.
//
// The renderer is data-oriented: it only ever sees plain instances + a
// few static meshes, so this module fully decouples the GL pipeline from
// any optimisation/CSV concerns.
//===----------------------------------------------------------------------===//

#include "../common/Types.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace warehouse {

/// One opaque, axis-aligned-ish box, ready to be rendered. Rotation is a
/// scalar Y-axis angle in radians (matches GLM convention).
struct BoxInstance {
    glm::vec3 position;      ///< bottom-left-floor corner BEFORE rotation
    glm::vec3 size;          ///< local-frame extents (w, h, d)
    float     rotationRadY;  ///< rotation around the Y (vertical) axis
    glm::vec3 color;         ///< base RGB color, [0,1]
};

/// Immutable bag of geometry handed off to the renderer.
struct Scene {
    // Camera / orientation hints.
    glm::vec3 worldCenter{0.0f}; ///< mid-AABB at floor level
    glm::vec3 worldExtents{0.0f};///< full size of the warehouse AABB

    // Static environment.
    /// Floor triangle list, clipped EXACTLY to the warehouse perimeter
    /// polygon (axis-aligned rectilinear decomposition).
    std::vector<glm::vec3> floorVertices;
    glm::vec4              floorColor{0.20f, 0.22f, 0.26f, 1.0f};

    /// Closed line loop for the perimeter (white outline above the floor).
    std::vector<glm::vec3> perimeterLineLoop;

    /// Translucent ceiling triangle list - one rectangle per (segment x
    /// polygon-cell), clipped to the polygon and elevated to the segment
    /// height. The renderer draws this with alpha blending.
    std::vector<glm::vec3> ceilingVertices;
    glm::vec4              ceilingColor{0.92f, 0.85f, 0.55f, 0.22f};

    std::vector<BoxInstance> obstacles;         ///< one per obstacle

    // Dynamic content.
    std::vector<BoxInstance> bays;              ///< one per placed bay

    /// Bright purple thin slabs sitting on the floor in front of each
    /// bay. Their bottom-left anchor is the world point local (0, depth)
    /// of the bay (i.e. the bay's "front-left" corner), rotated into
    /// world coordinates. The renderer draws them with the same outline
    /// pass as the bays.
    std::vector<BoxInstance> gaps;
    glm::vec4              gapColor{0.85f, 0.0f, 0.95f, 1.0f}; ///< neon magenta
};

class SceneBuilder {
public:
    /// Pure transformation - no GL calls happen here.
    /// `placements` should be the `Optimizer::Result::placements`.
    static Scene build(const WarehouseData& data,
                       const std::vector<PlacedBay>& placements);

private:
    static glm::vec3 colorForBayId(int id);

    /// Build a triangle list for the polygon, optionally elevated.
    /// Uses an axis-aligned rectilinear decomposition driven by the
    /// unique X / Y coordinates of `xCoords` / `yCoords`. A cell is
    /// emitted only if its centre lies strictly inside the polygon.
    static void buildPolygonMesh(const std::vector<WarehouseVertex>& poly,
                                 const std::vector<float>& xCoords,
                                 const std::vector<float>& yCoords,
                                 float yLevel,
                                 std::vector<glm::vec3>& outTris);

    /// Cell-elevation variant: the Y level is queried per-cell from the
    /// ceiling profile. Cells without a ceiling segment are skipped.
    static void buildCeilingMesh(const WarehouseData& data,
                                 std::vector<glm::vec3>& outTris);

    static bool pointInPolygon(float x, float y,
                               const std::vector<WarehouseVertex>& poly);
};

} // namespace warehouse

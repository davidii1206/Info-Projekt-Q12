/**
 * @file MeshCollisionBuilder.cpp
 * @brief Implementation of MeshCollisionBuilder.
 */

#include "MeshCollisionBuilder.h"
#include "../Graphics/Model.h"

// Jolt shape types
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>
#include <cassert>

JPH_SUPPRESS_WARNINGS
using namespace JPH;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{
    /**
     * @brief Transforms a glm::vec3 by a glm::mat4, returning a JPH::Float3.
     */
    inline JPH::Float3 TransformVertex(const glm::vec3& v, const glm::mat4& m)
    {
        glm::vec4 t = m * glm::vec4(v, 1.f);
        return JPH::Float3(t.x, t.y, t.z);
    }
} // namespace

// ---------------------------------------------------------------------------
// Build from ModelVertex + uint32_t indices
// ---------------------------------------------------------------------------

Shape::ShapeResult MeshCollisionBuilder::Build(
    const std::vector<ModelVertex>& vertices,
    const std::vector<uint32_t>&    indices,
    const glm::mat4&                transform)
{
    // Convert ModelVertex positions → glm::vec3 list, then delegate.
    std::vector<glm::vec3> positions;
    positions.reserve(vertices.size());
    for (const auto& v : vertices)
        positions.push_back(v.position);

    return Build(positions, indices, transform);
}

// ---------------------------------------------------------------------------
// Build from raw float3 positions + uint32_t indices
// ---------------------------------------------------------------------------

Shape::ShapeResult MeshCollisionBuilder::Build(
    const std::vector<glm::vec3>& positions,
    const std::vector<uint32_t>&  indices,
    const glm::mat4&              transform)
{
    if (positions.empty())
    {
        spdlog::error("[MeshCollisionBuilder] Empty vertex array – nothing to build.");
        Shape::ShapeResult result;
        result.SetError("Empty vertex array");
        return result;
    }

    if (indices.size() % 3 != 0)
    {
        spdlog::error("[MeshCollisionBuilder] Index count ({}) is not divisible by 3 – "
                      "mesh must be a triangle list.", indices.size());
        Shape::ShapeResult result;
        result.SetError("Index count not divisible by 3");
        return result;
    }

    const uint32_t triCount = static_cast<uint32_t>(indices.size() / 3);

    // Pre-allocate triangle array
    TriangleList triangles;
    triangles.reserve(triCount);

    const bool identity = (transform == glm::mat4(1.f));

    for (uint32_t t = 0; t < triCount; ++t)
    {
        const uint32_t i0 = indices[t * 3 + 0];
        const uint32_t i1 = indices[t * 3 + 1];
        const uint32_t i2 = indices[t * 3 + 2];

        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size())
        {
            spdlog::warn("[MeshCollisionBuilder] Triangle {} has out-of-range index "
                         "({}, {}, {}) – skipping.", t, i0, i1, i2);
            continue;
        }

        Triangle tri;
        if (identity)
        {
            const glm::vec3& v0 = positions[i0];
            const glm::vec3& v1 = positions[i1];
            const glm::vec3& v2 = positions[i2];
            tri.mV[0] = Float3(v0.x, v0.y, v0.z);
            tri.mV[1] = Float3(v1.x, v1.y, v1.z);
            tri.mV[2] = Float3(v2.x, v2.y, v2.z);
        }
        else
        {
            tri.mV[0] = TransformVertex(positions[i0], transform);
            tri.mV[1] = TransformVertex(positions[i1], transform);
            tri.mV[2] = TransformVertex(positions[i2], transform);
        }

        triangles.push_back(tri);
    }

    if (triangles.empty())
    {
        spdlog::error("[MeshCollisionBuilder] All triangles were skipped – "
                      "resulting shape would be empty.");
        Shape::ShapeResult result;
        result.SetError("No valid triangles");
        return result;
    }

    // Build the Jolt MeshShape
    MeshShapeSettings settings(std::move(triangles));
    settings.mMaxTrianglesPerLeaf = 4; // Good default for static environment

    Shape::ShapeResult result = settings.Create();
    if (!result.IsValid())
    {
        spdlog::error("[MeshCollisionBuilder] MeshShape creation failed: {}",
                      result.GetError().c_str());
    }
    else
    {
        spdlog::info("[MeshCollisionBuilder] Built MeshShape with {} triangles "
                     "from {} vertices.", triangles.size(), positions.size());
    }

    return result;
}

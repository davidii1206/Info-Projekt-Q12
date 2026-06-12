/**
 * @file MeshCollisionBuilder.h
 * @brief Utility for building Jolt Physics MeshShape from CPU-side vertex/index data.
 *
 * Usage:
 * @code
 *   auto sceneData = AssetManager::LoadGLTF("assets/test_scene.glb");
 *   auto shape = MeshCollisionBuilder::Build(
 *       sceneData.cpuVertices, sceneData.cpuIndices, glm::mat4(1.f));
 *   if (shape)
 *       physics->AddStaticMesh(std::move(shape));
 * @endcode
 */

#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

#include <glm/glm.hpp>
#include <vector>
#include <memory>

struct ModelVertex;

/**
 * @class MeshCollisionBuilder
 * @brief Static factory that converts raw CPU mesh data into a Jolt MeshShape.
 *
 * The builder triangulates the input index list (expects triangle lists),
 * applies an optional world transform, and returns a validated ShapeResult
 * that can be passed directly to PhysicsServer::AddStaticMesh().
 *
 * Performance notes:
 * - This is a one-time, load-time cost; not called per frame.
 * - For very large scenes (>100k tris) consider calling on a background thread
 *   before passing the result to the main thread.
 * - The builder always produces a MeshShape (exact, not convex-hull), which is
 *   correct for non-convex static environment geometry.
 */
class MeshCollisionBuilder
{
public:
    /**
     * @brief Builds a Jolt MeshShape from CPU vertex/index arrays.
     *
     * @param vertices  CPU-side vertex positions (only the position field is used).
     * @param indices   Index list for the mesh (triangle list; count must be divisible by 3).
     * @param transform Optional world-space transform applied to every vertex.
     *                  Pass glm::mat4(1.f) to keep the mesh in local space.
     * @return A valid JPH::Shape::ShapeResult. Check IsValid() before use.
     *         On failure, Error() contains a human-readable reason.
     */
    static JPH::Shape::ShapeResult Build(
        const std::vector<ModelVertex>& vertices,
        const std::vector<uint32_t>&    indices,
        const glm::mat4&                transform = glm::mat4(1.f));

    /**
     * @brief Convenience overload that works with raw float3 positions.
     *
     * Useful when only positions are available (no full ModelVertex).
     *
     * @param positions Flat list of XYZ positions.
     * @param indices   Triangle-list indices.
     * @param transform Optional world-space transform.
     * @return JPH::Shape::ShapeResult.
     */
    static JPH::Shape::ShapeResult Build(
        const std::vector<glm::vec3>& positions,
        const std::vector<uint32_t>&  indices,
        const glm::mat4&              transform = glm::mat4(1.f));

private:
    MeshCollisionBuilder() = delete;
};

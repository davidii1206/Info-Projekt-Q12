#pragma once
#include <entt/entt.hpp>
#include <cstdint>

class WorldManager;

/**
 * @struct StructurePlacementConfig
 * @brief Parameters controlling faction-base and neutral-node placement.
 */
struct StructurePlacementConfig {
    uint32_t seed         = 12345; ///< Deterministic placement seed.
    const char* baseModelPath = "assets/prim_sphere_red.glb";     ///< Model for faction bases.
    const char* nodeModelPath = "assets/prim_cylinder_brown.glb"; ///< Model for neutral nodes.
    float baseModelScale  = 2.5f;  ///< Scale applied to base models.
    float nodeModelScale  = 1.5f;  ///< Scale applied to node models.
    int   maxNeutralNodes = 16;    ///< Maximum number of neutral resource nodes.
    float nodeMinSpacing  = 15.0f; ///< Minimum world-unit radius between neutral nodes.
};

namespace StructurePlacementSystem {
    /// Places faction bases (one per territory) and neutral resource nodes.
    /// Returns total entity count placed.
    uint32_t Place(entt::registry& registry, const WorldManager& world,
                   const StructurePlacementConfig& cfg);

    /// Destroys all StructureComponent entities in the registry.
    void Clear(entt::registry& registry);
}

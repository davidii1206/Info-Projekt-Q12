#pragma once
#include <entt/entt.hpp>
#include <cstdint>

class WorldManager;

struct StructurePlacementConfig {
    uint32_t seed         = 12345;
    const char* baseModelPath = "assets/prim_sphere_red.glb";
    const char* nodeModelPath = "assets/prim_cylinder_brown.glb";
    float baseModelScale  = 2.5f;
    float nodeModelScale  = 1.5f;
    int   maxNeutralNodes = 16;
    float nodeMinSpacing  = 15.0f; // world-unit radius between neutral nodes
};

namespace StructurePlacementSystem {
    /// Places faction bases (one per territory) and neutral resource nodes.
    /// Returns total entity count placed.
    uint32_t Place(entt::registry& registry, const WorldManager& world,
                   const StructurePlacementConfig& cfg);

    /// Destroys all StructureComponent entities in the registry.
    void Clear(entt::registry& registry);
}

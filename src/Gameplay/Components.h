/**
 * @file Components.h
 * @brief Definition of ECS components used in the game world.
 */

#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include "../Core/PhysicsServer.h"
#include "../Graphics/Lights.h"   // LightComponent lives here

/**
 * @struct TransformComponent
 * @brief Holds position, rotation, and scale for an entity.
 */
struct TransformComponent {
    glm::vec3 position{0.f}; /**< The position of the entity in 3D space. */
    glm::vec3 rotation{0.f}; /**< The rotation of the entity in Euler angles. */
    glm::vec3 scale{1.f};    /**< The scale of the entity. */

    /**
     * @brief Default constructor.
     */
    TransformComponent() = default;

    /**
     * @brief Constructor with initial position.
     * @param pos The initial position.
     */
    explicit TransformComponent(glm::vec3 pos) : position(pos) {}
};

/**
 * @struct MovementComponent
 * @brief Data for entity movement.
 */
struct MovementComponent {
    glm::vec3 velocity{0.f}; /**< Current velocity vector. */
    float     speed = 10.f;  /**< Maximum movement speed. */
    glm::vec3 inputDir{0.f}; /**< XYZ direction set from input. */
};

/**
 * @struct PlayerComponent
 * @brief Tags an entity as a player.
 */
struct PlayerComponent {
    uint32_t playerId = 0;   /**< Unique ID for the player. */
    bool     isLocal  = false; /**< Whether this is the local player. */
};

/**
 * @struct NetworkedComponent
 * @brief Links an entity to a stable network identity.
 *
 * Client entities match server entities via this id.
 */
struct NetworkedComponent {
    uint32_t netId = 0; /**< Network-wide unique identifier. */
};

/**
 * @struct ModelComponent
 * @brief Stores the path to the 3D model asset.
 */
struct ModelComponent {
    std::string modelPath; /**< Path to the .glb or .gltf file. */
};

/**
 * @struct ScatterPropComponent
 * @brief Tags a purely decorative, client-side scattered prop (grass, rocks…).
 *
 * Scatter props are generated deterministically from the world seed on every
 * peer (see ScatterSystem), so they are NOT networked and carry no authoritative
 * state. The tag exists so the scatter pass can be cleared/regenerated without
 * touching gameplay entities.
 */
struct ScatterPropComponent {
    uint16_t layer = 0; /**< Index of the ScatterLayer that produced this prop. */
};

/**
 * @struct StructureComponent
 * @brief Tags a world structure (faction base or neutral resource node).
 *
 * Structures are placed deterministically by StructurePlacementSystem on every
 * peer, so they are not networked. isFactionBase=true marks the home plateau
 * marker for a territory; isFactionBase=false marks a neutral resource node.
 */
struct StructureComponent {
    uint16_t territoryId  = 0xFFFF; /**< Owning territory, or 0xFFFF for neutral. */
    bool     isFactionBase = false;
};

// ---------------------------------------------------------------------------
// Physics Components
// ---------------------------------------------------------------------------

/**
 * @struct PhysicsBodyComponent
 * @brief Links an entt entity with a Jolt physics body.
 */
struct PhysicsBodyComponent {
    PhysicsBodyHandle handle; /**< Handle to the physics server body. */
};

/**
 * @brief Links an ECS entity to a Jolt Physics body.
 */
struct PhysicsComponent {
    JPH::BodyID bodyID;
};

/**
 * @struct EntityIDComponent
 * @brief Optional tag for mapping world IDs back to entt entities.
 */
struct EntityIDComponent {
    uint32_t id = 0; /**< The unique entity ID. */
};

// ---------------------------------------------------------------------------
// NOTE: LightComponent is defined in Graphics/Lights.h and included above.
//       Add it to any entity that should emit light:
//
//   registry.emplace<TransformComponent>(e, glm::vec3{0, 5, 0});
//   registry.emplace<LightComponent>(e,
//       LightType::Point, glm::vec3{1,0.8f,0.4f}, 4.0f, 15.0f);
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Resource / Collector components — see ResourceTypes.h for full definitions.
// Included here so all ECS users get them transitively via Components.h.
// ---------------------------------------------------------------------------
#include "ResourceTypes.h"

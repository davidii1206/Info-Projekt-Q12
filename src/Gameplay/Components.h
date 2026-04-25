/**
 * @file Components.h
 * @brief Definition of ECS components used in the game world.
 */

#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include "../Core/PhysicsServer.h"

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

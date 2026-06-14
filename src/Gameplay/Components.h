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

// ---------------------------------------------------------------------------
// Unit / Combat Components
// ---------------------------------------------------------------------------

#include "Bug_classes.h"

/**
 * @struct UnitComponent
 * @brief Tags an entity as a controllable unit belonging to a team.
 */
struct UnitComponent {
    uint32_t teamId   = 0;                    ///< Owning team.
    BugClass bugClass = BugClass::Ants;       ///< Unit type / faction.
    bool     selected = false;                ///< Currently selected by Commander.
};

/**
 * @struct HealthComponent
 * @brief Tracks hit points for any entity that can take damage.
 */
struct HealthComponent {
    float hp    = 100.f; ///< Current hit points.
    float maxHp = 100.f; ///< Maximum hit points.
    bool  dead  = false; ///< True once hp <= 0.

    HealthComponent() = default;
    explicit HealthComponent(float max) : hp(max), maxHp(max) {}
};

/**
 * @struct CombatComponent
 * @brief Stores attack parameters for a unit.
 *
 * Damage scales by BugClass: carnivores deal more, omnivores medium, rest low.
 */
struct CombatComponent {
    float attackRange    = 5.f;   ///< Max distance to auto-attack an enemy.
    float attackDamage   = 10.f;  ///< Base damage per hit.
    float attackCooldown = 0.f;   ///< Remaining seconds until next attack.
    float attackRate     = 1.5f;  ///< Seconds between attacks.
    entt::entity target  = entt::null; ///< Current attack target (server).
};

/**
 * @struct MovementOrderComponent
 * @brief Carries a Commander-issued move order (right-click destination).
 */
struct MovementOrderComponent {
    glm::vec3 destination{0.f}; ///< World position to move towards.
    bool      active = false;   ///< Whether an order is pending.
};

/**
 * @struct BaseHealthComponent
 * @brief Marks an entity as a team base with health (destroyable).
 */
struct BaseHealthComponent {
    uint32_t teamId = 0;
    float    hp     = 500.f;
    float    maxHp  = 500.f;
    bool     destroyed = false;
};

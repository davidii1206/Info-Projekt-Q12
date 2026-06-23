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
#include "CameraMode.h"
#include "Bug_classes.h"
#include "Building_classes.h"

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
    CameraMode cameraMode = CameraMode::Commander; /**< Current camera mode. */
    BugClass bugClass = BugClass::None; /**< The faction/class of the player. */
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
    bool     isFactionBase = false; /**< True for a faction base marker, false for a neutral node. */
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
    JPH::BodyID bodyID; ///< The Jolt physics body backing this entity.
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

    HealthComponent() = default; ///< Default constructor.
    /// @brief Constructs full health with the given maximum.
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
    uint32_t teamId = 0;       ///< Owning team.
    float    hp     = 500.f;   ///< Current base hit points.
    float    maxHp  = 500.f;   ///< Maximum base hit points.
    bool     destroyed = false; ///< True once the base is destroyed.
};

// ---------------------------------------------------------------------------
// Ghost / Construction Components
// ---------------------------------------------------------------------------

/**
 * @struct GhostComponent
 * @brief Tags a client-only entity as a building-placement ghost (transparent,
 *        follows cursor, snapped to grid).
 */
struct GhostComponent {};

/**
 * @struct FogCoverComponent
 * @brief Tag for the fog-cover mesh entity (excluded from fog culling).
 */
struct FogCoverComponent {};

/**
 * @struct NoFogCullComponent
 * @brief Tag for entities that should never be fog-culled (e.g. terrain mesh).
 */
struct NoFogCullComponent {};

/**
 * @struct ConstructionComponent
 * @brief Building under construction — slides up from below ground over time.
 */
struct ConstructionComponent {
    float elapsed  = 0.f; ///< Elapsed construction time (seconds).
    float duration = 2.f; ///< Total slide-up duration (seconds).
    float startY   = 0.f; ///< Starting Y (below ground).
    float targetY  = 0.f; ///< Final Y (resting on terrain).
};

// ---------------------------------------------------------------------------
// Barracks / Breeding Chamber Components
// ---------------------------------------------------------------------------

/**
 * @struct BarracksComponent
 * @brief Tags a building as a barracks (Brutkammer) that can spawn units.
 *
 * Server-side only.  A separate system reads the spawn queue and produces
 * units over time.
 */
struct BarracksComponent {
    /// Units queued for production (each entry = unit type / tier).
    struct SpawnJob {
        uint8_t  tier    = 1;   ///< 1–5 unit tier
        float    timer   = 0.f; ///< Remaining production time
        float    total   = 5.f; ///< Total time for this job
    };

    std::vector<SpawnJob> queue;        ///< FIFO spawn queue
    float                 productionSpeed = 1.f; ///< Multiplier from tribe bonuses
    uint32_t              maxQueueSize    = 5;   ///< Max queued spawns
};

/**
 * @struct ConversionComponent
 * @brief Tags a building as a conversion chamber (Konversions-Kammer).
 *
 * Defines an input → output recipe that runs automatically while the
 * building has sufficient input resources in the team stockpile.
 */
struct ConversionComponent {
    ResourceType inputType  = ResourceType::Nektar; ///< Input resource type
    int          inputAmt   = 5;                     ///< Units consumed per cycle
    ResourceType outputType = ResourceType::Pilze;   ///< Output resource type
    int          outputAmt  = 3;                     ///< Units produced per cycle
    float        cycleTime  = 3.f;                   ///< Seconds per conversion cycle
    float        timer      = 0.f;                   ///< Remaining time
    bool         active     = false;                 ///< Currently converting
};

/**
 * @struct StorageComponent
 * @brief Tags a building as a resource silo (Speicher).
 *
 * Increases the maximum resource capacity for the owning team.
 * Multiple storage buildings stack multiplicatively or additively.
 */
struct StorageComponent {
    int capacityBonus = 50;   ///< Additional capacity per resource type
    bool isActive     = true; ///< False if the building is destroyed/under construction
};

/**
 * @struct SpecialBuildingComponent
 * @brief Tags a building as a tribe-specific special building.
 *
 * Contains runtime state for the special building's unique effect.
 * The exact semantics depend on `specialType`.
 */
struct SpecialBuildingComponent {
    SpecialBuildingType specialType;   ///< Which special building this is
    float               effectValue;   ///< Current effect strength (may scale with tier)
    float               effectRadius;  ///< Radius of effect
    float               cooldownTimer  = 0.f; ///< Ability cooldown (if applicable)
    float               cooldownTotal  = 0.f; ///< Total cooldown time
    bool                active         = true; ///< Whether the effect is running

    SpecialBuildingComponent() = default; ///< Default constructor.
    /// @brief Constructs from a special type, seeding effect values from its info.
    explicit SpecialBuildingComponent(SpecialBuildingType t)
        : specialType(t)
    {
        auto info = GetSpecialBuildingInfo(t);
        effectValue  = info.effectValue;
        effectRadius = info.effectRadius;
    }
};

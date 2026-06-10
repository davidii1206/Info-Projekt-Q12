/**
 * @file ResourceTypes.h
 * @brief Defines all resource types and ECS components for the resource system.
 *
 * Resources sind sammelbare Objekte in der Spielwelt (Pilze, Beeren, usw.).
 * Sie existieren als EnTT-Entities auf dem Server mit ResourceComponent und
 * TransformComponent. Permanente Ressourcen respawnen nach dem Einsammeln;
 * nicht-permanente (Drops) verschwinden nach einer konfigurierbaren Zeit.
 */

#pragma once
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// ResourceType
// ---------------------------------------------------------------------------

/**
 * @enum ResourceType
 * @brief All collectable resource categories in the game.
 */
enum class ResourceType : uint8_t {
    None    = 0,
    Pilze   = 1,  ///< Mushrooms  – found near trees / shaded areas
    Beeren  = 2,  ///< Berries    – bushes and clearings
    Nektar  = 3,  ///< Nectar     – flowers
    Samen   = 4,  ///< Seeds      – grass patches
    Insekten= 5,  ///< Insects    – anywhere, rare
    Fleisch = 6,  ///< Meat       – dropped by killed enemies/bosses
};

/**
 * @brief Returns a human-readable name for a ResourceType.
 */
inline const char* ResourceTypeName(ResourceType t) {
    switch (t) {
        case ResourceType::Pilze:    return "Pilze";
        case ResourceType::Beeren:   return "Beeren";
        case ResourceType::Nektar:   return "Nektar";
        case ResourceType::Samen:    return "Samen";
        case ResourceType::Insekten: return "Insekten";
        case ResourceType::Fleisch:  return "Fleisch";
        default:                     return "Unbekannt";
    }
}

// ---------------------------------------------------------------------------
// ECS Components
// ---------------------------------------------------------------------------

/**
 * @struct ResourceComponent
 * @brief Marks an entity as a collectable resource node.
 *
 * Attach this (alongside TransformComponent) to any entity that should
 * act as a resource pickup in the world.
 */
struct ResourceComponent {
    ResourceType type       = ResourceType::None; ///< What kind of resource this is.
    int          amount     = 1;                  ///< How many units can be collected.
    bool         permanent  = true;               ///< Respawns after collection if true.
    float        respawnTimer = 0.f;              ///< Counts down to respawn (seconds).
    float        respawnTime  = 30.f;             ///< Time until respawn after depletion.
    bool         depleted   = false;              ///< True while waiting to respawn.

    ResourceComponent() = default;
    ResourceComponent(ResourceType t, int amt, bool perm, float respawn = 30.f)
        : type(t), amount(amt), permanent(perm), respawnTime(respawn) {}
};

/**
 * @struct CollectorComponent
 * @brief Tags an entity as a unit that can collect resources.
 *
 * Entities with this component will automatically pick up ResourceComponents
 * within `collectRadius` and carry them to the base.
 */
struct CollectorComponent {
    float collectRadius   = 3.f;   ///< Distance within which resources are auto-collected.
    float collectCooldown = 0.f;   ///< Remaining cooldown before next collection.
    float collectRate     = 2.f;   ///< Seconds between collection attempts.
    bool  carryingLoad    = false; ///< True while returning to base with resources.
};

/**
 * @struct BaseComponent
 * @brief Marks an entity as a base / storage point for a team.
 *
 * Collectors return here after picking up resources.
 */
struct BaseComponent {
    uint32_t teamId = 0; ///< Which team owns this base.
};

/**
 * @struct ResourceInventory
 * @brief Stores how many of each resource a base or unit is holding.
 *
 * Attach to base entities to track global stockpile, or to collector
 * units to track what they are currently carrying.
 */
struct ResourceInventory {
    int pilze    = 0;
    int beeren   = 0;
    int nektar   = 0;
    int samen    = 0;
    int insekten = 0;
    int fleisch  = 0;

    /// Adds `amount` of the given type.
    void Add(ResourceType t, int amount) {
        switch (t) {
            case ResourceType::Pilze:    pilze    += amount; break;
            case ResourceType::Beeren:   beeren   += amount; break;
            case ResourceType::Nektar:   nektar   += amount; break;
            case ResourceType::Samen:    samen    += amount; break;
            case ResourceType::Insekten: insekten += amount; break;
            case ResourceType::Fleisch:  fleisch  += amount; break;
            default: break;
        }
    }

    /// Returns the stored amount for a given type.
    int Get(ResourceType t) const {
        switch (t) {
            case ResourceType::Pilze:    return pilze;
            case ResourceType::Beeren:   return beeren;
            case ResourceType::Nektar:   return nektar;
            case ResourceType::Samen:    return samen;
            case ResourceType::Insekten: return insekten;
            case ResourceType::Fleisch:  return fleisch;
            default:                     return 0;
        }
    }
};

/**
 * @file BuildingSystem.h
 * @brief System for handling building logic, such as upgrading, conversion, and spawning.
 */

#pragma once
#include <entt/entt.hpp>
#include "Building_classes.h"
#include "Components.h"

namespace BuildingSystem {
    /**
     * @brief Updates building states (upgrades, conversion, etc.)
     * @param registry The authoritative server registry.
     * @param dt Fixed delta time.
     */
    void Update(entt::registry& registry, float dt);

    /**
     * @brief Initiates an upgrade for a building if resources are available.
     * @param registry The registry containing the building and the player's base inventory.
     * @param buildingEntity The entity to upgrade.
     * @param baseEntity The base entity where resources are stored.
     * @return bool True if the upgrade was successfully started.
     */
    bool TryStartUpgrade(entt::registry& registry, entt::entity buildingEntity, entt::entity baseEntity);

    /**
     * @brief Processes conversion cycles for all ConversionComponent buildings.
     * @param registry The authoritative server registry.
     * @param dt Fixed delta time.
     */
    void UpdateConversions(entt::registry& registry, float dt);

    /**
     * @brief Processes barracks spawn queues.
     * @param registry The authoritative server registry.
     * @param dt Fixed delta time.
     */
    void UpdateBarracks(entt::registry& registry, float dt);

    /**
     * @brief Returns the effective storage capacity bonus for a team.
     * @param registry The registry containing StorageComponent buildings.
     * @param teamId The team to query.
     * @return Total capacity bonus (sum of all active storage buildings).
     */
    int GetTeamStorageBonus(entt::registry& registry, uint32_t teamId);

    /**
     * @brief Returns the default conversion recipe for a given BugClass.
     *
     * Bestimmt anhand des Ernährungstyps, welche Rohstoffe die
     * Konversions-Kammer standardmäßig umwandelt.
     */
    inline ConversionComponent GetDefaultConversion(BugClass bc) {
        switch (GetBugClassDiet(bc)) {
            // Herbivoren: Beeren → Nektar
            case DietType::Herbivore:
                return { ResourceType::Beeren, 3, ResourceType::Nektar, 2, 4.f, 0.f, false };
            // Karnivoren: Fleisch → Insekten
            case DietType::Carnivore:
                return { ResourceType::Fleisch, 2, ResourceType::Insekten, 3, 3.f, 0.f, false };
            // Allesfresser: Pilze → Beeren
            case DietType::Omnivore:
                return { ResourceType::Pilze, 3, ResourceType::Beeren, 2, 3.5f, 0.f, false };
            // Zersetzer: Samen → Pilze
            case DietType::Decomposer:
                return { ResourceType::Samen, 2, ResourceType::Pilze, 3, 3.f, 0.f, false };
            // Parasiten: Fleisch → Fleisch (Konservierung, 1:1 mit Bonus)
            case DietType::Parasite:
                return { ResourceType::Fleisch, 2, ResourceType::Fleisch, 3, 4.f, 0.f, false };
            default:
                return { ResourceType::Nektar, 3, ResourceType::Pilze, 2, 4.f, 0.f, false };
        }
    }
}

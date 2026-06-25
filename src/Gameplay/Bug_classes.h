/**
 * @file Bug_classes.h
 * @brief Enum definitions for the different bug factions/classes in the game.
 */

#pragma once
#include <cstdint>
#include <vector>
#include "ResourceTypes.h"

/**
 * @enum BugClass
 * @brief Maps bug factions to unique identifiers.
 */
enum class BugClass {
    None = 0,
    BeesWasps,          ///< Bienen & Wespen (Stecher-Allianz)
    ButterfliesMoths,   ///< Schmetterlinge & Raupen (Lepidoptera)
    Snails,             ///< Schnecken (Panzer-Konsortium)
    Mantis,             ///< Gottesanbeterinnen (Assassinen-Orden)
    Spiders,            ///< Spinnen (Seiden-Syndikat)
    Fireflies,          ///< Glühwürmchen (Licht-Kollektiv)
    Ants,               ///< Ameisen (Die Legion)
    Termites,           ///< Termiten (Baumeister)
    CentipedesWorms,    ///< Tausendfüßer & Würmer (Unterwelt-Gilde)
    MosquitosTicks,     ///< Mücken & Zecken (Die Plage)
    Woodlice,           ///< Asseln (Die Phalanx)
    Dragonflies,        ///< Libellen (Apex-Jäger)
    Bugs,               ///< Wanzen (Chemiewaffen-Kartell)
    Roaches,            ///< Schaben (Die Unsterblichen)
    Beetles,            ///< Großkäfer (Schwere Gladiatoren)
    Scorpions,          ///< Skorpione (Wüsten-Nomaden)
    BossArena           ///< Neutral boss territory — no player faction
};

// ---------------------------------------------------------------------------
// DietType – Ernährungstypen
// ---------------------------------------------------------------------------

/**
 * @enum DietType
 * @brief Klassifiziert, welche Art von Nahrung ein Stamm zu sich nimmt.
 *
 * Bestimmt, welche ResourceType-Entities ein Collector einsammeln darf
 * und welche Konversions-Rezepte standardmäßig zur Verfügung stehen.
 */
enum class DietType : uint8_t {
    Herbivore,    ///< Pflanzenfresser – Nektar, Beeren, Samen
    Carnivore,    ///< Fleischfresser – Insekten, Fleisch
    Omnivore,     ///< Allesfresser – alle Ressourcen
    Decomposer,   ///< Zersetzer – Pilze, totes organisches Material
    Parasite,     ///< Parasit/Blutsauger – Fleisch (Blut)
};

/**
 * @brief Returns the DietType for a given BugClass.
 */
inline DietType GetBugClassDiet(BugClass bc) {
    switch (bc) {
        // Herbivoren
        case BugClass::BeesWasps:        return DietType::Herbivore;
        case BugClass::ButterfliesMoths: return DietType::Herbivore;
        case BugClass::Snails:           return DietType::Herbivore;
        case BugClass::Bugs:             return DietType::Herbivore;
        // Karnivoren
        case BugClass::Mantis:           return DietType::Carnivore;
        case BugClass::Spiders:          return DietType::Carnivore;
        case BugClass::Fireflies:        return DietType::Carnivore;
        case BugClass::Dragonflies:      return DietType::Carnivore;
        case BugClass::Scorpions:        return DietType::Carnivore;
        // Allesfresser
        case BugClass::Ants:             return DietType::Omnivore;
        case BugClass::Roaches:          return DietType::Omnivore;
        case BugClass::Beetles:          return DietType::Omnivore;
        // Zersetzer
        case BugClass::Termites:         return DietType::Decomposer;
        case BugClass::CentipedesWorms:  return DietType::Decomposer;
        case BugClass::Woodlice:         return DietType::Decomposer;
        // Parasiten
        case BugClass::MosquitosTicks:   return DietType::Parasite;
        default:                         return DietType::Omnivore;
    }
}

/**
 * @brief Returns which ResourceTypes a BugClass can eat/collect.
 *
 * Jeder Ernährungstyp hat eine Liste von Ressourcen, die er sammeln kann.
 * Sammler ignorieren Ressourcen außerhalb dieser Liste.
 */
inline std::vector<ResourceType> GetEdibleResources(BugClass bc) {
    // Holz is a UNIVERSAL resource: every faction's workers chop wood for
    // base upgrades, regardless of diet. The diet-specific list is added on
    // top so each faction also collects food matching its biology.
    switch (GetBugClassDiet(bc)) {
        case DietType::Herbivore:
            return {ResourceType::Holz, ResourceType::Nektar, ResourceType::Beeren, ResourceType::Samen};
        case DietType::Carnivore:
            return {ResourceType::Holz, ResourceType::Insekten, ResourceType::Fleisch};
        case DietType::Omnivore:
            return {ResourceType::Holz, ResourceType::Pilze, ResourceType::Beeren, ResourceType::Nektar,
                    ResourceType::Samen, ResourceType::Insekten, ResourceType::Fleisch};
        case DietType::Decomposer:
            return {ResourceType::Holz, ResourceType::Pilze, ResourceType::Samen, ResourceType::Beeren};
        case DietType::Parasite:
            return {ResourceType::Holz, ResourceType::Fleisch};
        default:
            return {ResourceType::Holz};
    }
}

/**
 * @brief Returns the SIGNATURE resource for a faction — the single primary
 *        food/material that defines that faction. Used as:
 *          * the currency to buy additional workers (cost = 5 signature)
 *          * a +50% gather-speed bonus when collecting this type
 *          * the starting stockpile granted at game start
 */
inline ResourceType GetSignatureResource(BugClass bc) {
    switch (bc) {
        // Nektar-Sammler
        case BugClass::BeesWasps:        return ResourceType::Nektar;
        case BugClass::ButterfliesMoths: return ResourceType::Nektar;
        case BugClass::Snails:           return ResourceType::Nektar; // Pflanzensaefte → Nektar
        case BugClass::Bugs:             return ResourceType::Nektar; // Blattlaeuse → Pflanzensaefte → Nektar
        // Insekten-Jaeger
        case BugClass::Mantis:           return ResourceType::Insekten;
        case BugClass::Spiders:          return ResourceType::Insekten;
        case BugClass::Fireflies:        return ResourceType::Insekten;
        case BugClass::Dragonflies:      return ResourceType::Insekten;
        case BugClass::Scorpions:        return ResourceType::Insekten;
        // Allesfresser → Samen (most accessible / abundant)
        case BugClass::Ants:             return ResourceType::Samen;
        case BugClass::Roaches:          return ResourceType::Samen;
        case BugClass::Beetles:          return ResourceType::Samen;
        // Holz-Spezialist
        case BugClass::Termites:         return ResourceType::Holz;
        // Sonstige Zersetzer
        case BugClass::CentipedesWorms:  return ResourceType::Pilze;
        case BugClass::Woodlice:         return ResourceType::Pilze;
        // Blutsauger
        case BugClass::MosquitosTicks:   return ResourceType::Fleisch;
        default:                         return ResourceType::Pilze;
    }
}

/**
 * @brief Returns a human-readable label for the diet category.
 */
inline const char* DietTypeName(DietType d) {
    switch (d) {
        case DietType::Herbivore:   return "Pflanzenfresser";
        case DietType::Carnivore:   return "Fleischfresser";
        case DietType::Omnivore:    return "Allesfresser";
        case DietType::Decomposer:  return "Zersetzer";
        case DietType::Parasite:    return "Blutsauger";
        default:                    return "?";
    }
}

/**
 * @brief Returns true if the bug class can fly (ignores terrain height limits in pathfinding).
 */
inline bool IsFlying(BugClass bc) {
    switch (bc) {
        case BugClass::Dragonflies:
        case BugClass::MosquitosTicks:
        case BugClass::Fireflies:
        case BugClass::ButterfliesMoths:
        case BugClass::BeesWasps:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Returns true if the bug class can climb terraces directly
 *        (ignores tier differences — no ramps needed).
 *
 * Climbers are slower to compensate.
 */
inline bool IsClimber(BugClass bc) {
    switch (bc) {
        case BugClass::Snails:
        case BugClass::Spiders:
        case BugClass::Beetles:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Returns a human-readable name for a BugClass.
 */
inline const char* BugClassName(BugClass bc) {
    switch (bc) {
        case BugClass::None:             return "None";
        case BugClass::BeesWasps:        return "Stecher-Allianz";
        case BugClass::ButterfliesMoths: return "Lepidoptera";
        case BugClass::Snails:           return "Panzer-Konsortium";
        case BugClass::Mantis:           return "Assassinen-Orden";
        case BugClass::Spiders:          return "Seiden-Syndikat";
        case BugClass::Fireflies:        return "Licht-Kollektiv";
        case BugClass::Ants:             return "Legion";
        case BugClass::Termites:         return "Baumeister";
        case BugClass::CentipedesWorms:  return "Unterwelt-Gilde";
        case BugClass::MosquitosTicks:   return "Die Plage";
        case BugClass::Woodlice:         return "Die Phalanx";
        case BugClass::Dragonflies:      return "Apex-Jaeger";
        case BugClass::Bugs:             return "Chemiewaffen-Kartell";
        case BugClass::Roaches:          return "Die Urahnen";
        case BugClass::Beetles:          return "Schwere Gladiatoren";
        case BugClass::Scorpions:        return "Wespen-Monarchen";
        default:                         return "Unbekannt";
    }
}

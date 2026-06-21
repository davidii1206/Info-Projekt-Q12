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
    switch (GetBugClassDiet(bc)) {
        case DietType::Herbivore:
            return {ResourceType::Nektar, ResourceType::Beeren, ResourceType::Samen};
        case DietType::Carnivore:
            return {ResourceType::Insekten, ResourceType::Fleisch};
        case DietType::Omnivore:
            return {ResourceType::Pilze, ResourceType::Beeren, ResourceType::Nektar,
                    ResourceType::Samen, ResourceType::Insekten, ResourceType::Fleisch};
        case DietType::Decomposer:
            return {ResourceType::Pilze, ResourceType::Samen, ResourceType::Beeren};
        case DietType::Parasite:
            return {ResourceType::Fleisch};
        default:
            return {};
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

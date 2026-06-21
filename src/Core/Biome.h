#pragma once
#include "../Gameplay/Bug_classes.h"
#include <vector>

enum class BiomeType {
    None = 0,
    Wetland,
    Desert,
    MushroomForest,
    HiveGlade
};

inline BiomeType GetBiomeForBugClass(BugClass bugClass) {
    switch (bugClass) {
        case BugClass::Snails:
        case BugClass::MosquitosTicks:
        case BugClass::Dragonflies:
        case BugClass::Woodlice:
            return BiomeType::Wetland;

        case BugClass::Scorpions:
        case BugClass::Beetles:
        case BugClass::Roaches:
        case BugClass::Bugs:
            return BiomeType::Desert;

        case BugClass::Ants:
        case BugClass::Termites:
        case BugClass::CentipedesWorms:
        case BugClass::Spiders:
            return BiomeType::MushroomForest;

        case BugClass::BeesWasps:
        case BugClass::ButterfliesMoths:
        case BugClass::Mantis:
        case BugClass::Fireflies:
            return BiomeType::HiveGlade;

        default:
            return BiomeType::None;
    }
}

inline std::vector<BugClass> GetBugClassesForBiome(BiomeType biome) {
    switch (biome) {
        case BiomeType::Wetland:
            return { BugClass::Snails, BugClass::MosquitosTicks, BugClass::Dragonflies, BugClass::Woodlice };
        case BiomeType::Desert:
            return { BugClass::Scorpions, BugClass::Beetles, BugClass::Roaches, BugClass::Bugs };
        case BiomeType::MushroomForest:
            return { BugClass::Ants, BugClass::Termites, BugClass::CentipedesWorms, BugClass::Spiders };
        case BiomeType::HiveGlade:
            return { BugClass::BeesWasps, BugClass::ButterfliesMoths, BugClass::Mantis, BugClass::Fireflies };
        default:
            return {};
    }
}

inline const char* BiomeTypeToString(BiomeType biome) {
    switch (biome) {
        case BiomeType::Wetland: return "Wetland";
        case BiomeType::Desert: return "Desert";
        case BiomeType::MushroomForest: return "Mushroom Forest";
        case BiomeType::HiveGlade: return "Hive Glade";
        default: return "None";
    }
}

inline const char* BugClassToString(BugClass bugClass) {
    switch (bugClass) {
        case BugClass::Snails: return "Snails";
        case BugClass::MosquitosTicks: return "Mosquitos & Ticks";
        case BugClass::Dragonflies: return "Dragonflies";
        case BugClass::Woodlice: return "Woodlice";
        case BugClass::Scorpions: return "Scorpions";
        case BugClass::Beetles: return "Beetles";
        case BugClass::Roaches: return "Roaches";
        case BugClass::Bugs: return "Bugs";
        case BugClass::Ants: return "Ants";
        case BugClass::Termites: return "Termites";
        case BugClass::CentipedesWorms: return "Centipedes & Worms";
        case BugClass::Spiders: return "Spiders";
        case BugClass::BeesWasps: return "Bees & Wasps";
        case BugClass::ButterfliesMoths: return "Butterflies & Moths";
        case BugClass::Mantis: return "Mantis";
        case BugClass::Fireflies: return "Fireflies";
        default: return "None";
    }
}

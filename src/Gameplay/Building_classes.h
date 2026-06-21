/**
 * @file Building_classes.h
 * @brief Components and enums for the building system.
 * 
 * Enthält alle Gebäudetypen, stammesspezifische Spezialgebäude,
 * sowie die dazugehörigen Datenstrukturen für Kosten, Boni und Upgrades.
 */

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include "ResourceTypes.h"
#include "Bug_classes.h"

// ---------------------------------------------------------------------------
// BuildingType – Allgemeine Gebäudetypen
// ---------------------------------------------------------------------------

/**
 * @enum BuildingType
 * @brief Categorizes buildings by their primary function.
 * 
 * Jeder Stamm hat Zugriff auf alle allgemeinen Gebäudetypen (Main, Storage,
 * Barracks, Upgrade, Conversion, Defense, Attack, Outpost) sowie auf
 * 1–2 stammesspezifische Spezialgebäude (BuildingType::Special).
 */
enum class BuildingType : uint8_t {
    Main,           ///< Das Nest / Die Basis – Zentrale, schaltet Tiers frei
    Storage,        ///< Ressourcen-Silos – erhöht Ressourcen-Limit
    Barracks,       ///< Nährboden / Brutkammer – produziert Einheiten
    Upgrade,        ///< Evolutions-/Upgrade-Knoten – passive Buffs
    Conversion,     ///< Konversions-Kammer – Rohstoffumwandlung
    Defense,        ///< Verteidigungs-Strukturen – Türme, Fallen
    Attack,         ///< Offensiv-Gebäude – Angriffsboni
    Outpost,        ///< Vorposten – erweitert Territorium
    Special,        ///< Stammesspezifisches Spezialgebäude
    Count,          ///< Sentinel – number of building types (must be last)
};

/**
 * @brief Human-readable names for BuildingType.
 */
inline const char* BuildingTypeName(BuildingType t) {
    switch (t) {
        case BuildingType::Main:       return "Nest / Basis";
        case BuildingType::Storage:    return "Speicher";
        case BuildingType::Barracks:   return "Brutkammer";
        case BuildingType::Upgrade:    return "Upgrade-Knoten";
        case BuildingType::Conversion: return "Konversions-Kammer";
        case BuildingType::Defense:    return "Verteidigung";
        case BuildingType::Attack:     return "Angriff";
        case BuildingType::Outpost:    return "Vorposten";
        case BuildingType::Special:    return "Spezial";
        default:                       return "Unbekannt";
    }
}

// ---------------------------------------------------------------------------
// SpecialBuildingType – Stammesspezifische Spezialgebäude
// ---------------------------------------------------------------------------

/**
 * @enum SpecialBuildingType
 * @brief Defines every tribe-specific special building (1–2 per tribe).
 * 
 * Die Einträge sind nach Stämmen gruppiert. Jeder Eintrag entspricht
 * genau einem Gebäude, das nur von einem bestimmten Stamm gebaut werden kann.
 */
enum class SpecialBuildingType : uint8_t {
    // Stamm 1: Stecher-Allianz (Bienen & Wespen)
    NectarRefinery,       ///< Nektar → Honig (Spezialwährung)
    WaxWall,              ///< Hexagonale Mauer, reparierbar durch Flugeinheiten

    // Stamm 2: Lepidoptera (Schmetterlinge & Raupen)
    SilkManufactory,      ///< Produziert Fäden, verlangsamt Bodeneinheiten
    PollenDisperser,      ///< Turm: heilt Verbündete / verwirrt Feinde

    // Stamm 3: Panzer-Konsortium (Schnecken)
    SlimeCarpet,          ///< Verteilt Schleim um die Basis → verlangsamt Feinde
    LimeBulwark,          ///< Extrem widerstandsfähiger Verteidigungsturm

    // Stamm 4: Assassinen-Orden (Gottesanbeterinnen)
    ShadowHatchery,       ///< Macht frisch geschlüpfte Einheiten unsichtbar
    BladeSharpener,       ///< Erhöht kritischen Schaden aller Nahkämpfer

    // Stamm 5: Seiden-Syndikat (Spinnen)
    NetWeavery,           ///< Netzwerk für schnelle Bewegung eigener Einheiten
    CocoonPrison,         ///< Fängt Feinde → wandelt sie langsam in Ressourcen

    // Stamm 6: Licht-Kollektiv (Glühwürmchen)
    LightFocusTower,      ///< Laser-Turm
    BioluminescentRadar,  ///< Deckt getarnte Einheiten auf

    // Stamm 7: Legion (Ameisen)
    PheromonePost,        ///< Erhöht Angriffs- und Bewegungsgeschwindigkeit
    MassHatchery,         ///< Produziert Tier-1-Einheiten doppelt so schnell

    // Stamm 8: Baumeister (Termiten)
    RepairFermenter,      ///< Heilt Gebäude automatisch
    CementThrower,        ///< Turm: setzt Feinde fest

    // Stamm 9: Unterwelt-Gilde (Tausendfüßer & Würmer)
    TunnelHub,            ///< Teleportiert Einheiten durch die Basis
    EarthquakeGenerator,  ///< Destabilisiert feindliche Türme

    // Stamm 10: Die Plage (Mücken & Zecken)
    BloodPool,            ///< Speichert geraubte Blut-Ressource
    DiseaseIncubator,     ///< Infiziert angreifende Feinde

    // Stamm 11: Die Phalanx (Asseln)
    ShieldProjector,      ///< Projektil-Barriere über der Basis
    GroundAnchor,         ///< Verhindert Wegstoßen von Einheiten

    // Stamm 12: Apex-Jäger (Libellen)
    WindTunnel,           ///< Geschwindigkeitsschub für Flugeinheiten
    DivePerches,          ///< Erhöht Sichtweite

    // Stamm 13: Chemiewaffen-Kartell (Wanzen)
    StinkGasVent,         ///< Giftwolke um die Basis
    AcidDepot,            ///< Erhöht Giftschaden aller Einheiten

    // Stamm 14: Die Urahnen (Roaches / Ur-Insekten)
    AmberMonolith,        ///< Friert Feinde bei Zerstörung ein

    // Stamm 15: Schwere Gladiatoren (Hirschkäfer / Beetles)
    FightingArena,        ///< Trainiert Einheiten (Erfahrung)
    BatteringRamForge,    ///< Erhöht Gebäudeschaden

    // Stamm 16: Wespen-Monarchen (Hornissen / Scorpions)
    DeathZoneBanner,      ///< Negiert feindliche Verteidigung im Zielgebiet
    HuntHeadquarters,     ///< Markiert stärkstes Monster → Boni bei Tod

    Count,                ///< Anzahl der Spezialgebäude (für Iteration)
};

/**
 * @brief Human-readable names for SpecialBuildingType (DE/EN gemischt).
 */
inline const char* SpecialBuildingTypeName(SpecialBuildingType t) {
    switch (t) {
        case SpecialBuildingType::NectarRefinery:      return "Nektar-Raffinerie";
        case SpecialBuildingType::WaxWall:             return "Wachswall";
        case SpecialBuildingType::SilkManufactory:     return "Seiden-Manufaktur";
        case SpecialBuildingType::PollenDisperser:     return "Pollen-Disperser";
        case SpecialBuildingType::SlimeCarpet:         return "Schleim-Teppich";
        case SpecialBuildingType::LimeBulwark:         return "Kalk-Bollwerk";
        case SpecialBuildingType::ShadowHatchery:      return "Schatten-Brutstätte";
        case SpecialBuildingType::BladeSharpener:      return "Klingenschärfer";
        case SpecialBuildingType::NetWeavery:          return "Netz-Weberei";
        case SpecialBuildingType::CocoonPrison:        return "Kokon-Gefängnis";
        case SpecialBuildingType::LightFocusTower:     return "Licht-Fokus-Turm";
        case SpecialBuildingType::BioluminescentRadar: return "Biolumineszenter Radar";
        case SpecialBuildingType::PheromonePost:       return "Pheromon-Posten";
        case SpecialBuildingType::MassHatchery:        return "Massen-Brutkammer";
        case SpecialBuildingType::RepairFermenter:     return "Reparatur-Fermenter";
        case SpecialBuildingType::CementThrower:       return "Zement-Werfer";
        case SpecialBuildingType::TunnelHub:           return "Tunnel-Knotenpunkt";
        case SpecialBuildingType::EarthquakeGenerator: return "Erdbeben-Generator";
        case SpecialBuildingType::BloodPool:           return "Blut-Pool";
        case SpecialBuildingType::DiseaseIncubator:    return "Seuchen-Inkubator";
        case SpecialBuildingType::ShieldProjector:     return "Schild-Projektor";
        case SpecialBuildingType::GroundAnchor:        return "Boden-Anker";
        case SpecialBuildingType::WindTunnel:          return "Windkanal";
        case SpecialBuildingType::DivePerches:         return "Sturzflug-Barsche";
        case SpecialBuildingType::StinkGasVent:        return "Stinkgas-Schlot";
        case SpecialBuildingType::AcidDepot:           return "Säure-Depot";
        case SpecialBuildingType::AmberMonolith:       return "Bernstein-Monolith";
        case SpecialBuildingType::FightingArena:       return "Kampf-Arena";
        case SpecialBuildingType::BatteringRamForge:   return "Rammbock-Schmiede";
        case SpecialBuildingType::DeathZoneBanner:     return "Todeszonen-Banner";
        case SpecialBuildingType::HuntHeadquarters:    return "Jagd-Zentrale";
        default:                                       return "Unbekannt";
    }
}

// ---------------------------------------------------------------------------
// UpgradeRequirement
// ---------------------------------------------------------------------------

/**
 * @struct UpgradeRequirement
 * @brief Defines the cost to upgrade a building to the next tier.
 */
struct UpgradeRequirement {
    ResourceInventory cost;       ///< Resource cost of the upgrade.
    float buildTime = 5.0f;       ///< Seconds to complete the upgrade.
};

// ---------------------------------------------------------------------------
// TribeBuildingData – Stammesspezifische Gebäudeboni
// ---------------------------------------------------------------------------

/**
 * @struct TribeBuildingData
 * @brief Holds all building-related bonuses and special building unlocks for one tribe.
 * 
 * Wird verwendet, um beim Bau und Upgrade die Kosten, HP und Effekte
 * stammesabhängig zu skalieren.
 */
struct TribeBuildingData {
    // Kostenmultiplikatoren (1.0 = Standard)
    float storageCostMul      = 1.0f; ///< Storage building cost multiplier.
    float barracksCostMul     = 1.0f; ///< Barracks building cost multiplier.
    float upgradeCostMul      = 1.0f; ///< Upgrade building cost multiplier.
    float conversionCostMul   = 1.0f; ///< Conversion building cost multiplier.
    float defenseCostMul      = 1.0f; ///< Defense building cost multiplier.
    float attackCostMul       = 1.0f; ///< Attack building cost multiplier.
    float outpostCostMul      = 1.0f; ///< Outpost building cost multiplier.

    // Stat-Multiplikatoren
    float storageCapacityMul  = 1.0f; ///< Erhöhtes Ressourcen-Limit
    float unitProductionSpeed = 1.0f; ///< schnellere Einheitenproduktion
    float upgradeSpeedMul     = 1.0f; ///< schnellere Upgrades
    float defenseHpMul        = 1.0f; ///< Mehr HP für Verteidigungsgebäude
    float attackDmgMul        = 1.0f; ///< Mehr Angriffsschaden

    // Spezialgebäude, die dieser Stamm freischaltet (1–2 Stück)
    SpecialBuildingType specialA = SpecialBuildingType::NectarRefinery; ///< First special building this tribe unlocks.
    SpecialBuildingType specialB = SpecialBuildingType::NectarRefinery; ///< Second special (may equal specialA = only one).
    bool hasSpecialB = false; ///< Whether the tribe has a distinct second special building.
};

/**
 * @brief Returns the building-data configuration for a given BugClass.
 * 
 * Definiert für alle 16 Stämme ihre spezifischen Boni und Spezialgebäude.
 * 
 * @param bc Der Stamm.
 * @return Das zugehörige TribeBuildingData.
 */
inline TribeBuildingData GetTribeBuildingData(BugClass bc) {
    TribeBuildingData d;
    switch (bc) {
        // ---- Stamm 1: Stecher-Allianz (Bienen & Wespen) ----
        case BugClass::BeesWasps:
            d.conversionCostMul = 0.8f;
            d.attackCostMul     = 0.8f;
            d.unitProductionSpeed = 1.1f;
            d.attackDmgMul      = 1.1f;
            d.specialA = SpecialBuildingType::NectarRefinery;
            d.specialB = SpecialBuildingType::WaxWall;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 2: Lepidoptera (Schmetterlinge & Raupen) ----
        case BugClass::ButterfliesMoths:
            d.upgradeCostMul    = 0.85f;
            d.conversionCostMul = 0.9f;
            d.unitProductionSpeed = 0.9f;
            d.upgradeSpeedMul   = 1.2f;
            d.specialA = SpecialBuildingType::SilkManufactory;
            d.specialB = SpecialBuildingType::PollenDisperser;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 3: Panzer-Konsortium (Schnecken) ----
        case BugClass::Snails:
            d.defenseCostMul    = 0.75f;
            d.defenseHpMul      = 1.5f;
            d.unitProductionSpeed = 0.8f;
            d.upgradeSpeedMul   = 0.9f;
            d.specialA = SpecialBuildingType::SlimeCarpet;
            d.specialB = SpecialBuildingType::LimeBulwark;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 4: Assassinen-Orden (Gottesanbeterinnen) ----
        case BugClass::Mantis:
            d.attackCostMul     = 0.9f;
            d.attackDmgMul      = 1.25f;
            d.specialA = SpecialBuildingType::ShadowHatchery;
            d.specialB = SpecialBuildingType::BladeSharpener;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 5: Seiden-Syndikat (Spinnen) ----
        case BugClass::Spiders:
            d.defenseCostMul    = 0.9f;
            d.conversionCostMul = 0.85f;
            d.attackDmgMul      = 0.9f;
            d.specialA = SpecialBuildingType::NetWeavery;
            d.specialB = SpecialBuildingType::CocoonPrison;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 6: Licht-Kollektiv (Glühwürmchen) ----
        case BugClass::Fireflies:
            d.attackCostMul     = 0.85f;
            d.attackDmgMul      = 1.15f;
            d.specialA = SpecialBuildingType::LightFocusTower;
            d.specialB = SpecialBuildingType::BioluminescentRadar;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 7: Legion (Ameisen) ----
        case BugClass::Ants:
            d.barracksCostMul     = 0.7f;
            d.unitProductionSpeed = 1.5f;
            d.attackDmgMul        = 0.8f;
            d.storageCostMul      = 0.9f;
            d.specialA = SpecialBuildingType::PheromonePost;
            d.specialB = SpecialBuildingType::MassHatchery;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 8: Baumeister (Termiten) ----
        case BugClass::Termites:
            d.defenseCostMul    = 0.8f;
            d.storageCostMul    = 0.8f;
            d.storageCapacityMul= 1.25f;
            d.defenseHpMul      = 1.3f;
            d.unitProductionSpeed = 0.9f;
            d.specialA = SpecialBuildingType::RepairFermenter;
            d.specialB = SpecialBuildingType::CementThrower;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 9: Unterwelt-Gilde (Tausendfüßer & Würmer) ----
        case BugClass::CentipedesWorms:
            d.outpostCostMul    = 0.75f;
            d.defenseHpMul      = 1.15f;
            d.attackDmgMul      = 0.9f;
            d.specialA = SpecialBuildingType::TunnelHub;
            d.specialB = SpecialBuildingType::EarthquakeGenerator;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 10: Die Plage (Mücken & Zecken) ----
        case BugClass::MosquitosTicks:
            d.conversionCostMul = 0.8f;
            d.attackDmgMul      = 1.1f;
            d.defenseHpMul      = 0.9f;
            d.specialA = SpecialBuildingType::BloodPool;
            d.specialB = SpecialBuildingType::DiseaseIncubator;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 11: Die Phalanx (Asseln) ----
        case BugClass::Woodlice:
            d.defenseCostMul    = 0.85f;
            d.defenseHpMul      = 1.75f;
            d.attackDmgMul      = 0.7f;
            d.unitProductionSpeed = 0.85f;
            d.upgradeSpeedMul   = 0.9f;
            d.specialA = SpecialBuildingType::ShieldProjector;
            d.specialB = SpecialBuildingType::GroundAnchor;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 12: Apex-Jäger (Libellen) ----
        case BugClass::Dragonflies:
            d.attackCostMul     = 0.9f;
            d.attackDmgMul      = 1.2f;
            d.defenseHpMul      = 0.85f;
            d.unitProductionSpeed = 1.15f;
            d.specialA = SpecialBuildingType::WindTunnel;
            d.specialB = SpecialBuildingType::DivePerches;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 13: Chemiewaffen-Kartell (Wanzen) ----
        case BugClass::Bugs:
            d.attackCostMul     = 0.9f;
            d.attackDmgMul      = 1.15f;
            d.defenseCostMul    = 1.1f;
            d.conversionCostMul = 0.85f;
            d.specialA = SpecialBuildingType::StinkGasVent;
            d.specialB = SpecialBuildingType::AcidDepot;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 14: Die Urahnen (Roaches / Ur-Insekten) ----
        case BugClass::Roaches:
            d.upgradeCostMul    = 1.15f;
            d.defenseHpMul      = 1.25f;
            d.attackDmgMul      = 1.3f;
            d.unitProductionSpeed = 0.75f;
            d.upgradeSpeedMul   = 0.8f;
            d.specialA = SpecialBuildingType::AmberMonolith;
            d.hasSpecialB = false;
            break;

        // ---- Stamm 15: Schwere Gladiatoren (Hirschkäfer / Beetles) ----
        case BugClass::Beetles:
            d.attackCostMul     = 0.85f;
            d.attackDmgMul      = 1.3f;
            d.defenseHpMul      = 1.2f;
            d.unitProductionSpeed = 0.9f;
            d.storageCapacityMul  = 0.9f;
            d.specialA = SpecialBuildingType::FightingArena;
            d.specialB = SpecialBuildingType::BatteringRamForge;
            d.hasSpecialB = true;
            break;

        // ---- Stamm 16: Wespen-Monarchen (Hornissen / Scorpions) ----
        case BugClass::Scorpions:
            d.attackCostMul     = 0.85f;
            d.attackDmgMul      = 1.2f;
            d.defenseHpMul      = 0.9f;
            d.unitProductionSpeed = 1.1f;
            d.specialA = SpecialBuildingType::DeathZoneBanner;
            d.specialB = SpecialBuildingType::HuntHeadquarters;
            d.hasSpecialB = true;
            break;

        default:
            break;
    }
    return d;
}

// ---------------------------------------------------------------------------
// SpecialBuildingInfo – Statische Daten für jedes Spezialgebäude
// ---------------------------------------------------------------------------

/**
 * @struct SpecialBuildingInfo
 * @brief Statische Metadaten für ein stammesspezifisches Spezialgebäude.
 */
struct SpecialBuildingInfo {
    SpecialBuildingType type;        ///< Eindeutige ID
    const char*         name;        ///< Anzeigename
    const char*         description; ///< Kurzbeschreibung
    BugClass            tribe;       ///< Nur dieser Stamm kann es bauen
    float               baseHp;      ///< Basis-Lebenspunkte
    ResourceInventory   buildCost;   ///< Baukosten
    float               buildTime;   ///< Bauzeit in Sekunden
    float               effectValue; ///< Numerischer Effektwert (z.B. Schaden, Heilung)
    float               effectRadius;///< Wirkradius
};

/**
 * @brief Returns the static SpecialBuildingInfo for a given SpecialBuildingType.
 */
inline SpecialBuildingInfo GetSpecialBuildingInfo(SpecialBuildingType type) {
    switch (type) {
        // Stamm 1: Stecher-Allianz
        case SpecialBuildingType::NectarRefinery:
            return { type, "Nektar-Raffinerie", "Wandelt Nektar in Honig (Spezialwährung) um",
                     BugClass::BeesWasps, 300.f,
                     ResourceInventory{20, 10, 50, 0, 0, 0}, 8.f, 2.f, 10.f };
        case SpecialBuildingType::WaxWall:
            return { type, "Wachswall", "Hexagonale Mauer – fliegende Einheiten reparieren sie",
                     BugClass::BeesWasps, 800.f,
                     ResourceInventory{0, 0, 30, 20, 0, 0}, 6.f, 5.f, 0.f };

        // Stamm 2: Lepidoptera
        case SpecialBuildingType::SilkManufactory:
            return { type, "Seiden-Manufaktur", "Produziert Fäden, die Bodeneinheiten verlangsamen",
                     BugClass::ButterfliesMoths, 250.f,
                     ResourceInventory{10, 20, 10, 30, 0, 0}, 7.f, 0.5f, 12.f };
        case SpecialBuildingType::PollenDisperser:
            return { type, "Pollen-Disperser", "Versprüht Sporen: heilt Verbündete, verwirrt Feinde",
                     BugClass::ButterfliesMoths, 350.f,
                     ResourceInventory{10, 30, 20, 10, 0, 0}, 8.f, 15.f, 8.f };

        // Stamm 3: Panzer-Konsortium
        case SpecialBuildingType::SlimeCarpet:
            return { type, "Schleim-Teppich", "Zäher Schleim verlangsamt Feinde extrem",
                     BugClass::Snails, 400.f,
                     ResourceInventory{30, 10, 0, 10, 0, 0}, 6.f, 0.8f, 15.f };
        case SpecialBuildingType::LimeBulwark:
            return { type, "Kalk-Bollwerk", "Massiver Turm – kaum zerstörbar",
                     BugClass::Snails, 1500.f,
                     ResourceInventory{50, 20, 0, 30, 0, 0}, 12.f, 0.f, 0.f };

        // Stamm 4: Assassinen-Orden
        case SpecialBuildingType::ShadowHatchery:
            return { type, "Schatten-Brutstätte", "Frisch geschlüpfte Einheiten werden unsichtbar",
                     BugClass::Mantis, 200.f,
                     ResourceInventory{20, 10, 0, 15, 10, 0}, 6.f, 5.f, 10.f };
        case SpecialBuildingType::BladeSharpener:
            return { type, "Klingenschärfer", "Erhöht kritischen Schaden aller Nahkämpfer",
                     BugClass::Mantis, 250.f,
                     ResourceInventory{10, 0, 0, 20, 10, 10}, 7.f, 0.3f, 0.f };

        // Stamm 5: Seiden-Syndikat
        case SpecialBuildingType::NetWeavery:
            return { type, "Netz-Weberei", "Generiert Netzwerk für schnelle Bewegung",
                     BugClass::Spiders, 300.f,
                     ResourceInventory{20, 10, 0, 30, 0, 0}, 8.f, 2.f, 20.f };
        case SpecialBuildingType::CocoonPrison:
            return { type, "Kokon-Gefängnis", "Fängt besiegte Feinde und wandelt sie in Ressourcen",
                     BugClass::Spiders, 350.f,
                     ResourceInventory{30, 10, 0, 10, 10, 0}, 7.f, 1.f, 5.f };

        // Stamm 6: Licht-Kollektiv
        case SpecialBuildingType::LightFocusTower:
            return { type, "Licht-Fokus-Turm", "Bündelt Licht zu einem Laserstrahl",
                     BugClass::Fireflies, 400.f,
                     ResourceInventory{10, 20, 30, 10, 0, 0}, 9.f, 25.f, 12.f };
        case SpecialBuildingType::BioluminescentRadar:
            return { type, "Biolumineszenter Radar", "Deckt getarnte Einheiten im Umkreis auf",
                     BugClass::Fireflies, 200.f,
                     ResourceInventory{10, 10, 20, 10, 0, 0}, 5.f, 0.f, 25.f };

        // Stamm 7: Legion
        case SpecialBuildingType::PheromonePost:
            return { type, "Pheromon-Posten", "Erhöht Angriffs- und Bewegungsgeschwindigkeit im Radius",
                     BugClass::Ants, 250.f,
                     ResourceInventory{20, 20, 10, 10, 0, 0}, 6.f, 1.3f, 12.f };
        case SpecialBuildingType::MassHatchery:
            return { type, "Massen-Brutkammer", "Produziert Tier-1-Einheiten doppelt so schnell",
                     BugClass::Ants, 300.f,
                     ResourceInventory{30, 10, 0, 10, 0, 0}, 7.f, 2.f, 0.f };

        // Stamm 8: Baumeister
        case SpecialBuildingType::RepairFermenter:
            return { type, "Reparatur-Fermenter", "Heilt beschädigte Gebäude automatisch",
                     BugClass::Termites, 350.f,
                     ResourceInventory{30, 10, 0, 20, 0, 0}, 8.f, 5.f, 15.f };
        case SpecialBuildingType::CementThrower:
            return { type, "Zement-Werfer", "Versiegt Feinde mit Brei – setzt sie fest",
                     BugClass::Termites, 450.f,
                     ResourceInventory{40, 10, 0, 20, 10, 0}, 9.f, 3.f, 10.f };

        // Stamm 9: Unterwelt-Gilde
        case SpecialBuildingType::TunnelHub:
            return { type, "Tunnel-Knotenpunkt", "Teleportiert Einheiten durch die Basis",
                     BugClass::CentipedesWorms, 300.f,
                     ResourceInventory{30, 10, 0, 10, 10, 0}, 10.f, 0.f, 30.f };
        case SpecialBuildingType::EarthquakeGenerator:
            return { type, "Erdbeben-Generator", "Destabilisiert feindliche Fernkampftürme",
                     BugClass::CentipedesWorms, 250.f,
                     ResourceInventory{20, 0, 0, 10, 10, 10}, 8.f, 15.f, 15.f };

        // Stamm 10: Die Plage
        case SpecialBuildingType::BloodPool:
            return { type, "Blut-Pool", "Speichert geraubte Blut-Ressource für mächtige Buffs",
                     BugClass::MosquitosTicks, 400.f,
                     ResourceInventory{10, 0, 0, 10, 20, 10}, 7.f, 50.f, 0.f };
        case SpecialBuildingType::DiseaseIncubator:
            return { type, "Seuchen-Inkubator", "Infiziert angreifende Feinde mit einer Seuche",
                     BugClass::MosquitosTicks, 300.f,
                     ResourceInventory{20, 0, 0, 10, 10, 10}, 7.f, 5.f, 10.f };

        // Stamm 11: Die Phalanx
        case SpecialBuildingType::ShieldProjector:
            return { type, "Schild-Projektor", "Erzeugt eine Barriere, die Projektile abfängt",
                     BugClass::Woodlice, 500.f,
                     ResourceInventory{40, 20, 0, 20, 0, 0}, 10.f, 0.f, 15.f };
        case SpecialBuildingType::GroundAnchor:
            return { type, "Boden-Anker", "Verhindert Wegstoßen von Einheiten",
                     BugClass::Woodlice, 200.f,
                     ResourceInventory{20, 10, 0, 10, 0, 0}, 5.f, 0.f, 10.f };

        // Stamm 12: Apex-Jäger
        case SpecialBuildingType::WindTunnel:
            return { type, "Windkanal", "Erzeugt Aufwind für Flugeinheiten (Geschwindigkeit)",
                     BugClass::Dragonflies, 250.f,
                     ResourceInventory{10, 10, 20, 10, 0, 0}, 6.f, 2.f, 10.f };
        case SpecialBuildingType::DivePerches:
            return { type, "Sturzflug-Barsche", "Erhöht die Sichtweite dramatisch",
                     BugClass::Dragonflies, 200.f,
                     ResourceInventory{10, 10, 10, 10, 0, 0}, 5.f, 50.f, 0.f };

        // Stamm 13: Chemiewaffen-Kartell
        case SpecialBuildingType::StinkGasVent:
            return { type, "Stinkgas-Schlot", "Permanente Giftwolke um die Basis",
                     BugClass::Bugs, 350.f,
                     ResourceInventory{20, 0, 10, 20, 10, 0}, 7.f, 8.f, 12.f };
        case SpecialBuildingType::AcidDepot:
            return { type, "Säure-Depot", "Erhöht den Giftschaden aller Einheiten",
                     BugClass::Bugs, 250.f,
                     ResourceInventory{20, 10, 0, 10, 10, 0}, 6.f, 1.5f, 0.f };

        // Stamm 14: Die Urahnen
        case SpecialBuildingType::AmberMonolith:
            return { type, "Bernstein-Monolith", "Friert Feinde bei Zerstörung in Harz ein",
                     BugClass::Roaches, 600.f,
                     ResourceInventory{40, 20, 10, 20, 10, 0}, 12.f, 5.f, 10.f };

        // Stamm 15: Schwere Gladiatoren
        case SpecialBuildingType::FightingArena:
            return { type, "Kampf-Arena", "Trainiert Einheiten – gibt Erfahrung",
                     BugClass::Beetles, 400.f,
                     ResourceInventory{30, 10, 0, 20, 10, 10}, 9.f, 1.f, 10.f };
        case SpecialBuildingType::BatteringRamForge:
            return { type, "Rammbock-Schmiede", "Erhöht Schaden gegen Gebäude",
                     BugClass::Beetles, 300.f,
                     ResourceInventory{20, 0, 0, 20, 10, 20}, 8.f, 1.5f, 0.f };

        // Stamm 16: Wespen-Monarchen
        case SpecialBuildingType::DeathZoneBanner:
            return { type, "Todeszonen-Banner", "Negiert feindliche Verteidigung im Zielgebiet",
                     BugClass::Scorpions, 350.f,
                     ResourceInventory{20, 10, 10, 20, 10, 0}, 8.f, 0.f, 15.f };
        case SpecialBuildingType::HuntHeadquarters:
            return { type, "Jagd-Zentrale", "Markiert stärkstes Monster – Boni bei Tod",
                     BugClass::Scorpions, 300.f,
                     ResourceInventory{20, 10, 10, 10, 10, 10}, 7.f, 2.f, 50.f };

        default:
            return { type, "Unbekannt", "?", BugClass::None, 100.f, {}, 5.f, 0.f, 0.f };
    }
}

/**
 * @brief Returns the array of special buildings a tribe can build.
 */
inline std::vector<SpecialBuildingType> GetSpecialBuildingsForTribe(BugClass bc) {
    std::vector<SpecialBuildingType> result;
    auto data = GetTribeBuildingData(bc);
    result.push_back(data.specialA);
    if (data.hasSpecialB && data.specialB != data.specialA)
        result.push_back(data.specialB);
    return result;
}

// ---------------------------------------------------------------------------
// BuildingComponent
// ---------------------------------------------------------------------------

/**
 * @struct BuildingComponent
 * @brief Primary component for building logic, HP, and tiers.
 * 
 * Für Special-Gebäude enthält `specialType` den Spezialtyp.
 */
struct BuildingComponent {
    BuildingType type;       ///< General building category.
    BugClass ownerClass;     ///< Tribe that owns the building (drives bonuses).
    uint32_t teamId = 0;     ///< Owning team.
    uint32_t tier  = 1;      ///< Current tier/level (legacy field, see currentTier).

    /// Für BuildingType::Special: der genaue Spezialgebäudetyp.
    SpecialBuildingType specialType = SpecialBuildingType::NectarRefinery;

    float hp    = 100.0f;    ///< Current hit points.
    float maxHp = 100.0f;    ///< Maximum hit points.

    bool destroyed = false;  ///< True once the building has been destroyed.

    uint32_t currentTier = 1; ///< Current upgrade tier.
    uint32_t maxTier = 3;     ///< Highest reachable tier.

    bool isUpgrading      = false; ///< True while an upgrade is in progress.
    float upgradeTimer    = 0.0f;  ///< Seconds remaining on the current upgrade.
    float currentUpgradeTime = 0.0f; ///< Total duration of the current upgrade.
    bool upgradedThisTick = false; ///< Set the tick an upgrade completes (for broadcast).

    BuildingComponent() = default;
    /// @brief Constructs a general building (defaults specialType to NectarRefinery).
    BuildingComponent(BuildingType t, uint32_t tid, uint32_t tier_, float initialHp, float maxInitialHp, bool destroyed_)
        : type(t), ownerClass(BugClass::Termites), teamId(tid), tier(tier_),
          specialType(SpecialBuildingType::NectarRefinery),
          hp(initialHp), maxHp(maxInitialHp), destroyed(destroyed_) {}

    /// @brief Constructs a building with an explicit special-building type.
    BuildingComponent(BuildingType t, uint32_t tid, uint32_t tier_,
                      SpecialBuildingType st_, float initialHp, float maxInitialHp, bool destroyed_)
        : type(t), ownerClass(BugClass::Termites), teamId(tid), tier(tier_),
          specialType(st_),
          hp(initialHp), maxHp(maxInitialHp), destroyed(destroyed_) {}

    /**
     * @brief Returns the tribe-adjusted cost multiplier for a building type.
     */
    static float GetCostMultiplier(BugClass bc, BuildingType type) {
        auto data = GetTribeBuildingData(bc);
        switch (type) {
            case BuildingType::Storage:    return data.storageCostMul;
            case BuildingType::Barracks:   return data.barracksCostMul;
            case BuildingType::Upgrade:    return data.upgradeCostMul;
            case BuildingType::Conversion: return data.conversionCostMul;
            case BuildingType::Defense:    return data.defenseCostMul;
            case BuildingType::Attack:     return data.attackCostMul;
            case BuildingType::Outpost:    return data.outpostCostMul;
            default:                       return 1.0f;
        }
    }

    /**
     * @brief Gets the upgrade requirements for the next tier.
     * 
     * @return UpgradeRequirement for next tier.
     */
    static UpgradeRequirement GetNextTierCost(BugClass bc, BuildingType type, uint32_t nextTier) {
        UpgradeRequirement req;
        if (nextTier > 3) return req;

        int baseCost = static_cast<int>(nextTier) * 10;
        float mul = GetCostMultiplier(bc, type);

        switch (type) {
            case BuildingType::Main:
                req.cost.pilze = static_cast<int>(baseCost * 3 * mul);
                req.cost.beeren = static_cast<int>(baseCost * 2 * mul);
                req.buildTime = 15.0f * nextTier;
                break;
            case BuildingType::Storage:
                req.cost.pilze = static_cast<int>(baseCost * 2 * mul);
                req.cost.beeren = static_cast<int>(baseCost * mul);
                req.buildTime = 5.0f * nextTier;
                break;
            case BuildingType::Barracks:
                req.cost.fleisch = static_cast<int>(baseCost * 2 * mul);
                req.cost.nektar = static_cast<int>(baseCost * mul);
                req.buildTime = 8.0f * nextTier;
                break;
            case BuildingType::Upgrade:
                req.cost.samen = static_cast<int>(baseCost * 2 * mul);
                req.cost.pilze = static_cast<int>(baseCost * mul);
                req.buildTime = 12.0f * nextTier;
                break;
            case BuildingType::Conversion:
                req.cost.nektar = static_cast<int>(baseCost * 2 * mul);
                req.cost.beeren = static_cast<int>(baseCost * mul);
                req.buildTime = 7.0f * nextTier;
                break;
            case BuildingType::Defense:
                req.cost.pilze = static_cast<int>(baseCost * 2 * mul);
                req.cost.samen = static_cast<int>(baseCost * mul);
                req.buildTime = 10.0f * nextTier;
                break;
            case BuildingType::Attack:
                req.cost.fleisch = static_cast<int>(baseCost * 2 * mul);
                req.cost.samen = static_cast<int>(baseCost * mul);
                req.buildTime = 8.0f * nextTier;
                break;
            case BuildingType::Outpost:
                req.cost.pilze = static_cast<int>(baseCost * mul);
                req.cost.nektar = static_cast<int>(baseCost * mul);
                req.buildTime = 12.0f * nextTier;
                break;
            case BuildingType::Special:
                req.cost.pilze = baseCost * 3;
                req.cost.fleisch = baseCost * 2;
                req.buildTime = 10.0f * nextTier;
                break;
        }
        return req;
    }
};

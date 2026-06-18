/**
 * @file Bug_classes.h
 * @brief Enum definitions for the different bug factions/classes in the game.
 */

#pragma once

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

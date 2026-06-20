#include "UpgradeDefs.h"

// ---------------------------------------------------------------------------
// Base upgrades (id 1–99)
// ---------------------------------------------------------------------------
// Level 1→2 (ids 1–3)
static const UpgradePathDef kPath_Military = {
    1, "Militaer", "Kill 5 Feinde. Gibt +20% Einheitenschaden, schaltet Angriffs-Upgrades frei.",
    BuildingType::Count,
    {
        {UpgradeReqType::KillCount, {}, 5}
    },
    {1.2f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, BuildingType::Attack, SpecialBuildingType::Count}
};

static const UpgradePathDef kPath_Economic = {
    2, "Wirtschaft", "Besitze 50 Pilze & 50 Beeren. Gibt +20% Sammelrate, schaltet Lager-Upgrades frei.",
    BuildingType::Count,
    {
        {UpgradeReqType::Resources, {50, 50, 0, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.2f, 1.0f, 1.0f, 1.0f, BuildingType::Storage, SpecialBuildingType::Count}
};

static const UpgradePathDef kPath_Fortification = {
    3, "Befestigung", "Baue 5 Gebaeude. Gibt +30% Gebaeude-HP, schaltet Verteidigungs-Upgrades frei.",
    BuildingType::Count,
    {
        {UpgradeReqType::BuildingCount, {}, 5, BuildingType::Main}
    },
    {1.0f, 1.0f, 1.0f, 1.3f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, BuildingType::Defense, SpecialBuildingType::Count}
};

// Level 2→3 (ids 4–6)
static const UpgradePathDef kPath_War = {
    4, "Krieg", "Benötigt 'Militaer' + 20 Kills. Gibt +40% Einheitenschaden & +30% Brutrate.",
    BuildingType::Count,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 1},
        {UpgradeReqType::KillCount, {}, 20}
    },
    {1.4f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.3f, 1.0f}
};

static const UpgradePathDef kPath_Trade = {
    5, "Handel", "Benötigt 'Wirtschaft' + 100 Pilze & 100 Beeren. Gibt +40% Sammelrate & +50% Lager.",
    BuildingType::Count,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 2},
        {UpgradeReqType::Resources, {100, 100, 0, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.4f, 1.5f, 1.0f, 1.0f}
};

static const UpgradePathDef kPath_Fortress = {
    6, "Festung", "Benötigt 'Befestigung' + 15 Gebaeude. Gibt +50% Gebaeude-HP & -20% Baukosten.",
    BuildingType::Count,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 3},
        {UpgradeReqType::BuildingCount, {}, 15}
    },
    {1.0f, 1.0f, 1.0f, 1.5f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f}
};

// ---------------------------------------------------------------------------
// Building upgrades (id 100+)
// ---------------------------------------------------------------------------

// Storage (ids 100–102)
static const UpgradePathDef k_Storage_1 = {
    100, "Lager I", "+100 Lagerkapazitaet. Kostet 30 Pilze, 20 Beeren.",
    BuildingType::Storage,
    {{UpgradeReqType::Resources, {30, 20, 0, 0, 0, 0}}},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Storage_2 = {
    101, "Lager II", "+250 Lagerkapazitaet. Benötigt Lager I + 60 Pilze, 40 Beeren.",
    BuildingType::Storage,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 100},
        {UpgradeReqType::Resources, {60, 40, 0, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Storage_3 = {
    102, "Lager III", "+500 Lagerkapazitaet. Benötigt Lager II + 120 Pilze, 80 Beeren.",
    BuildingType::Storage,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 101},
        {UpgradeReqType::Resources, {120, 80, 0, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};

// Barracks (ids 110–115)
static const UpgradePathDef k_Barracks_Speed1 = {
    110, "Schnellbrut I", "+30% Brutgeschwindigkeit. Kostet 40 Fleisch, 30 Nektar.",
    BuildingType::Barracks,
    {{UpgradeReqType::Resources, {0, 0, 30, 0, 0, 40}}},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.3f, 1.0f}
};
static const UpgradePathDef k_Barracks_Speed2 = {
    111, "Schnellbrut II", "+60% Brutgeschwindigkeit. Benötigt Schnellbrut I.",
    BuildingType::Barracks,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 110},
        {UpgradeReqType::Resources, {0, 0, 60, 0, 0, 80}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.6f, 1.0f}
};
static const UpgradePathDef k_Barracks_Speed3 = {
    112, "Schnellbrut III", "+100% Brutgeschwindigkeit. Benötigt Schnellbrut II.",
    BuildingType::Barracks,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 111},
        {UpgradeReqType::Resources, {0, 0, 120, 0, 0, 160}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 1.0f}
};

static const UpgradePathDef k_Barracks_Elite1 = {
    113, "Elite I", "+15% Einheiten-HP & Schaden. Benötigt 10 Kills.",
    BuildingType::Barracks,
    {{UpgradeReqType::KillCount, {}, 10}},
    {1.15f, 1.15f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Barracks_Elite2 = {
    114, "Elite II", "+30% Einheiten-HP & Schaden. Benötigt Elite I + 25 Kills.",
    BuildingType::Barracks,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 113},
        {UpgradeReqType::KillCount, {}, 25}
    },
    {1.3f, 1.3f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Barracks_Elite3 = {
    115, "Elite III", "+50% Einheiten-HP & Schaden. Benötigt Elite II + 50 Kills.",
    BuildingType::Barracks,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 114},
        {UpgradeReqType::KillCount, {}, 50}
    },
    {1.5f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};

// Conversion (ids 120–122)
static const UpgradePathDef k_Conversion_1 = {
    120, "Konversion I", "+40% Konversionsrate. Kostet 30 Nektar, 20 Beeren.",
    BuildingType::Conversion,
    {{UpgradeReqType::Resources, {0, 20, 30, 0, 0, 0}}},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.4f}
};
static const UpgradePathDef k_Conversion_2 = {
    121, "Konversion II", "+80% Konversionsrate. Benötigt Konversion I.",
    BuildingType::Conversion,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 120},
        {UpgradeReqType::Resources, {0, 40, 60, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.8f}
};
static const UpgradePathDef k_Conversion_3 = {
    122, "Konversion III", "+150% Konversionsrate. Benötigt Konversion II.",
    BuildingType::Conversion,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 121},
        {UpgradeReqType::Resources, {0, 80, 120, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.5f}
};

// Defense (ids 130–132)
static const UpgradePathDef k_Defense_1 = {
    130, "Verstaerkung I", "+30% Verteidigungs-HP. Kostet 40 Pilze, 30 Samen.",
    BuildingType::Defense,
    {{UpgradeReqType::Resources, {40, 0, 0, 30, 0, 0}}},
    {1.0f, 1.0f, 1.0f, 1.3f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Defense_2 = {
    131, "Verstaerkung II", "+50% Verteidigungs-HP. Benötigt Verstärkung I.",
    BuildingType::Defense,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 130},
        {UpgradeReqType::Resources, {80, 0, 0, 60, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Defense_3 = {
    132, "Verstaerkung III", "+100% Verteidigungs-HP. Benötigt Verstärkung II.",
    BuildingType::Defense,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 131},
        {UpgradeReqType::Resources, {160, 0, 0, 120, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};

// Attack (ids 140–142)
static const UpgradePathDef k_Attack_1 = {
    140, "Schwere Geschosse I", "+30% Angriffsschaden. Kostet 50 Fleisch, 30 Samen.",
    BuildingType::Attack,
    {{UpgradeReqType::Resources, {0, 0, 0, 30, 0, 50}}},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Attack_2 = {
    141, "Schwere Geschosse II", "+60% Angriffsschaden. Benötigt Schwere Geschosse I.",
    BuildingType::Attack,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 140},
        {UpgradeReqType::Resources, {0, 0, 0, 60, 0, 100}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Attack_3 = {
    142, "Schwere Geschosse III", "+100% Angriffsschaden. Benötigt Schwere Geschosse II.",
    BuildingType::Attack,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 141},
        {UpgradeReqType::Resources, {0, 0, 0, 120, 0, 200}}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};

// Upgrade building (ids 150–152)
static const UpgradePathDef k_Upgrade_1 = {
    150, "Forschung I", "+30% Forschungsrate. Benötigt 10 Kills.",
    BuildingType::Upgrade,
    {{UpgradeReqType::KillCount, {}, 10}},
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Upgrade_2 = {
    151, "Forschung II", "+60% Forschungsrate. Benötigt Forschung I + 25 Kills.",
    BuildingType::Upgrade,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 150},
        {UpgradeReqType::KillCount, {}, 25}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Upgrade_3 = {
    152, "Forschung III", "+100% Forschungsrate. Benötigt Forschung II + 50 Kills.",
    BuildingType::Upgrade,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 151},
        {UpgradeReqType::KillCount, {}, 50}
    },
    {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};

// Outpost (ids 160–162)
static const UpgradePathDef k_Outpost_1 = {
    160, "Ausbau I", "+30% Vorposten-HP. Kostet 30 Pilze, 30 Nektar.",
    BuildingType::Outpost,
    {{UpgradeReqType::Resources, {30, 0, 30, 0, 0, 0}}},
    {1.0f, 1.0f, 1.0f, 1.3f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Outpost_2 = {
    161, "Ausbau II", "+60% Vorposten-HP. Benötigt Ausbau I.",
    BuildingType::Outpost,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 160},
        {UpgradeReqType::Resources, {60, 0, 60, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 1.6f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};
static const UpgradePathDef k_Outpost_3 = {
    162, "Ausbau III", "+100% Vorposten-HP. Benötigt Ausbau II.",
    BuildingType::Outpost,
    {
        {UpgradeReqType::HasUpgrade, {}, 0, BuildingType::Main, 161},
        {UpgradeReqType::Resources, {120, 0, 120, 0, 0, 0}}
    },
    {1.0f, 1.0f, 1.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}
};

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
static const UpgradePathDef* const kAllPaths[] = {
    &kPath_Military, &kPath_Economic, &kPath_Fortification,
    &kPath_War, &kPath_Trade, &kPath_Fortress,
    &k_Storage_1, &k_Storage_2, &k_Storage_3,
    &k_Barracks_Speed1, &k_Barracks_Speed2, &k_Barracks_Speed3,
    &k_Barracks_Elite1, &k_Barracks_Elite2, &k_Barracks_Elite3,
    &k_Conversion_1, &k_Conversion_2, &k_Conversion_3,
    &k_Defense_1, &k_Defense_2, &k_Defense_3,
    &k_Attack_1, &k_Attack_2, &k_Attack_3,
    &k_Upgrade_1, &k_Upgrade_2, &k_Upgrade_3,
    &k_Outpost_1, &k_Outpost_2, &k_Outpost_3,
};

const std::vector<UpgradePathDef>& GetAllUpgradeDefs() {
    static std::vector<UpgradePathDef> vec;
    if (vec.empty()) {
        for (auto* p : kAllPaths)
            vec.push_back(*p);
    }
    return vec;
}

const UpgradePathDef* FindUpgradeDef(UpgradePathID id) {
    for (auto* p : kAllPaths) {
        if (p->id == id) return p;
    }
    return nullptr;
}

std::vector<const UpgradePathDef*> GetBaseUpgradesForLevel(int level) {
    std::vector<const UpgradePathDef*> result;
    for (auto* p : kAllPaths) {
        if (p->id <= 99 && (p->appliesTo == BuildingType::Count || p->appliesTo == BuildingType::Main)) {
            bool isLevel2 = (p->id >= 1 && p->id <= 3);
            bool isLevel3 = (p->id >= 4 && p->id <= 6);
            if ((level == 2 && isLevel2) || (level == 3 && isLevel3))
                result.push_back(p);
        }
    }
    return result;
}

std::vector<const UpgradePathDef*> GetBuildingUpgradesForType(BuildingType type) {
    std::vector<const UpgradePathDef*> result;
    for (auto* p : kAllPaths) {
        if (p->id >= 100 && p->appliesTo == type)
            result.push_back(p);
    }
    return result;
}

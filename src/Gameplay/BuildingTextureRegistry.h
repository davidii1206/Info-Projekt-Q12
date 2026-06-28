/**
 * @file BuildingTextureRegistry.h
 * @brief Lädt stammes- und gebäudespezifische Texturen inkl. Schadenszustände.
 *
 * Erwartete Dateistruktur (unter src/Assets/, wird beim Build nach assets/ kopiert):
 *
 *   buildings/construction_sheet.png          – 8×8 Sprite-Sheet (64 Frames) für Bau-Animation
 *   buildings/{stamm}/{gebaeude}.png          – gesund (100–66 % HP)
 *   buildings/{stamm}/{gebaeude}_damaged.png  – beschädigt (66–33 % HP)
 *   buildings/{stamm}/{gebaeude}_critical.png – kritisch (<33 % HP)
 *
 * {stamm} = bees_wasps, termites, …  (siehe BugClassSlug)
 * {gebaeude} = main, storage, barracks, … oder special_nectar_refinery, …
 */

#pragma once

#include "Building_classes.h"
#include "Bug_classes.h"
#include "../Graphics/API/Texture.h"
#include <SDL3/SDL_gpu.h>
#include <memory>
#include <string>
#include <unordered_map>

enum class BuildingDamageState : uint8_t {
    Healthy     = 0,  // >75% HP
    Damaged     = 1,  // 50–75% HP
    HeavyDamage = 2,  // 25–50% HP
    Critical    = 3,  // 10–25% HP
    Destroyed   = 4,  // ≤10% HP
};

inline BuildingDamageState GetBuildingDamageState(float hp, float maxHp) {
    if (maxHp <= 0.f) return BuildingDamageState::Healthy;
    const float ratio = hp / maxHp;
    if (ratio > 0.75f) return BuildingDamageState::Healthy;
    if (ratio > 0.50f) return BuildingDamageState::Damaged;
    if (ratio > 0.25f) return BuildingDamageState::HeavyDamage;
    if (ratio > 0.10f) return BuildingDamageState::Critical;
    return BuildingDamageState::Destroyed;
}

const char* BugClassSlug(BugClass bc);
const char* BuildingTypeSlug(BuildingType type, SpecialBuildingType special);
std::string BuildingTexturePath(BugClass tribe, BuildingType type,
                                SpecialBuildingType special, BuildingDamageState damage);

class BuildingTextureRegistry {
public:
    void Load(SDL_GPUDevice* device);
    void Clear();

    std::shared_ptr<Texture> GetTexture(BugClass tribe, BuildingType type,
                                          SpecialBuildingType special,
                                          BuildingDamageState damage);

    std::shared_ptr<Texture> GetConstructionSheet() const { return m_ConstructionSheet; }

private:
    std::shared_ptr<Texture> LoadOrGenerate(SDL_GPUDevice* device, const std::string& path,
                                              BugClass tribe, BuildingType type,
                                              SpecialBuildingType special,
                                              BuildingDamageState damage);
    std::shared_ptr<Texture> GeneratePlaceholder(SDL_GPUDevice* device, BugClass tribe,
                                                   BuildingType type,
                                                   SpecialBuildingType special,
                                                   BuildingDamageState damage);
    std::shared_ptr<Texture> GenerateConstructionSheet(SDL_GPUDevice* device);

    SDL_GPUDevice* m_Device = nullptr;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_Textures;
    std::shared_ptr<Texture> m_ConstructionSheet;
};

namespace BuildingTextures {
    inline BuildingTextureRegistry& Get() {
        static BuildingTextureRegistry instance;
        return instance;
    }

    inline void Load(SDL_GPUDevice* device) { Get().Load(device); }
    inline void Unload() { Get().Clear(); }
}

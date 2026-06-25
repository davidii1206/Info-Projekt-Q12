/**
 * @file BuildingTextureRegistry.cpp
 * @brief Implementation of building texture loading and procedural fallbacks.
 */

#include "BuildingTextureRegistry.h"
#include "../Core/AssetManager.h"
#include <glm/glm.hpp>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <vector>
#include <stb_image_write.h>
#include <fstream>

namespace {

// Forward-Deklaration (Definition weiter unten im namespace)
glm::u8vec4 TribeBaseColor(BugClass tribe);

// ---------------------------------------------------------------------------
// Template-PNG Generierung (512×512, RGBA opak)
// Zeigt Rand, Grid und Eckmarkierungen als Mal-Vorlage.
// ---------------------------------------------------------------------------
static void SaveBuildingTemplate(const std::string& path, BugClass tribe) {
    constexpr int SIZE   = 512;
    constexpr int BORDER = 16;   // dunkler Außenrand = Kante des Cube-Face
    constexpr int INNER  = 24;   // hellerer innerer Sicherheitsrand
    constexpr int GRID   = 64;   // Gitter-Abstand

    std::vector<uint8_t> px(SIZE * SIZE * 4);

    // Stammes-Grundfarbe, etwas aufgehellt für Sichtbarkeit
    glm::u8vec4 base = TribeBaseColor(tribe);
    auto lighten = [](uint8_t c, int d) -> uint8_t {
        return (uint8_t)glm::clamp((int)c + d, 0, 255);
    };
    uint8_t br = lighten(base.r, 55), bg = lighten(base.g, 55), bb = lighten(base.b, 55);

    auto set = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || x >= SIZE || y < 0 || y >= SIZE) return;
        int i = (y * SIZE + x) * 4;
        px[i]=r; px[i+1]=g; px[i+2]=b; px[i+3]=255;
    };

    // 1. Hintergrund mit leichtem Licht-Verlauf (oben-links heller)
    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            float light = 1.f - (x + y) / float(SIZE * 2) * 0.12f;
            set(x, y,
                (uint8_t)(br * light),
                (uint8_t)(bg * light),
                (uint8_t)(bb * light));
        }
    }

    // 2. Gitterlinien (halbdunkel)
    for (int g = INNER; g <= SIZE - INNER; g += GRID) {
        for (int i = INNER; i <= SIZE - INNER; ++i) {
            uint8_t gr = (uint8_t)(br * 0.65f), gg = (uint8_t)(bg * 0.65f), gb_ = (uint8_t)(bb * 0.65f);
            set(g, i, gr, gg, gb_);
            set(i, g, gr, gg, gb_);
        }
    }

    // 3. Innerer Sicherheitsrand (zeigt nutzbaren Bereich)
    for (int i = 0; i < SIZE; ++i) {
        uint8_t mr = (uint8_t)(br*0.55f), mg=(uint8_t)(bg*0.55f), mb=(uint8_t)(bb*0.55f);
        set(INNER,       i, mr,mg,mb);
        set(SIZE-INNER,  i, mr,mg,mb);
        set(i, INNER,    mr,mg,mb);
        set(i, SIZE-INNER,mr,mg,mb);
    }

    // 4. Dicker dunkler Außenrand = sichtbare Kante des Cube-Face
    for (int b = 0; b < BORDER; ++b) {
        for (int i = 0; i < SIZE; ++i) {
            set(b,          i,  22,18,18);
            set(SIZE-1-b,   i,  22,18,18);
            set(i,          b,  22,18,18);
            set(i, SIZE-1-b,    22,18,18);
        }
    }

    // 5. Kreuzmarkierungen an allen Gitter-Kreuzungspunkten
    for (int gx = INNER; gx <= SIZE - INNER; gx += GRID) {
        for (int gy = INNER; gy <= SIZE - INNER; gy += GRID) {
            for (int d = -3; d <= 3; ++d) {
                set(gx+d, gy,   10,10,10);
                set(gx,   gy+d, 10,10,10);
            }
        }
    }

    // 6. Mittelkreuz (Zentrierung)
    for (int d = -12; d <= 12; ++d) {
        set(SIZE/2+d, SIZE/2, 10,10,10);
        set(SIZE/2, SIZE/2+d, 10,10,10);
    }
    // Mittelpunkt-Markierung (5×5 Quadrat)
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx)
            set(SIZE/2+dx, SIZE/2+dy, 10,10,10);

    stbi_write_png(path.c_str(), SIZE, SIZE, 4, px.data(), SIZE * 4);
}

glm::u8vec4 TribeBaseColor(BugClass tribe) {
    switch (tribe) {
        case BugClass::BeesWasps:        return {220, 190, 60, 255};
        case BugClass::ButterfliesMoths: return {180, 120, 210, 255};
        case BugClass::Snails:           return {90, 150, 110, 255};
        case BugClass::Mantis:           return {70, 140, 80, 255};
        case BugClass::Spiders:          return {120, 70, 150, 255};
        case BugClass::Fireflies:        return {60, 180, 220, 255};
        case BugClass::Ants:             return {180, 140, 70, 255};
        case BugClass::Termites:         return {150, 110, 70, 255};
        case BugClass::CentipedesWorms:  return {110, 80, 60, 255};
        case BugClass::MosquitosTicks:   return {130, 170, 90, 255};
        case BugClass::Woodlice:         return {140, 150, 160, 255};
        case BugClass::Dragonflies:      return {50, 150, 180, 255};
        case BugClass::Bugs:             return {150, 180, 50, 255};
        case BugClass::Roaches:          return {120, 90, 70, 255};
        case BugClass::Beetles:          return {90, 70, 110, 255};
        case BugClass::Scorpions:        return {200, 120, 50, 255};
        default:                         return {160, 160, 160, 255};
    }
}

} // namespace

const char* BugClassSlug(BugClass bc) {
    switch (bc) {
        case BugClass::BeesWasps:        return "bees_wasps";
        case BugClass::ButterfliesMoths: return "butterflies_moths";
        case BugClass::Snails:           return "snails";
        case BugClass::Mantis:           return "mantis";
        case BugClass::Spiders:          return "spiders";
        case BugClass::Fireflies:        return "fireflies";
        case BugClass::Ants:             return "ants";
        case BugClass::Termites:         return "termites";
        case BugClass::CentipedesWorms:  return "centipedes_worms";
        case BugClass::MosquitosTicks:   return "mosquitos_ticks";
        case BugClass::Woodlice:         return "woodlice";
        case BugClass::Dragonflies:      return "dragonflies";
        case BugClass::Bugs:             return "bugs";
        case BugClass::Roaches:          return "roaches";
        case BugClass::Beetles:          return "beetles";
        case BugClass::Scorpions:        return "scorpions";
        default:                         return "default";
    }
}

const char* BuildingTypeSlug(BuildingType type, SpecialBuildingType special) {
    switch (type) {
        case BuildingType::Main:       return "main";
        case BuildingType::Storage:    return "storage";
        case BuildingType::Barracks:   return "barracks";
        case BuildingType::Upgrade:    return "upgrade";
        case BuildingType::Conversion: return "conversion";
        case BuildingType::Defense:    return "defense";
        case BuildingType::Attack:     return "attack";
        case BuildingType::Outpost:    return "outpost";
        case BuildingType::Special: {
            switch (special) {
                case SpecialBuildingType::NectarRefinery:      return "special_nectar_refinery";
                case SpecialBuildingType::WaxWall:             return "special_wax_wall";
                case SpecialBuildingType::SilkManufactory:     return "special_silk_manufactory";
                case SpecialBuildingType::PollenDisperser:     return "special_pollen_disperser";
                case SpecialBuildingType::SlimeCarpet:         return "special_slime_carpet";
                case SpecialBuildingType::LimeBulwark:         return "special_lime_bulwark";
                case SpecialBuildingType::ShadowHatchery:      return "special_shadow_hatchery";
                case SpecialBuildingType::BladeSharpener:      return "special_blade_sharpener";
                case SpecialBuildingType::NetWeavery:          return "special_net_weavery";
                case SpecialBuildingType::CocoonPrison:        return "special_cocoon_prison";
                case SpecialBuildingType::LightFocusTower:     return "special_light_focus_tower";
                case SpecialBuildingType::BioluminescentRadar: return "special_bioluminescent_radar";
                case SpecialBuildingType::PheromonePost:       return "special_pheromone_post";
                case SpecialBuildingType::MassHatchery:      return "special_mass_hatchery";
                case SpecialBuildingType::RepairFermenter:     return "special_repair_fermenter";
                case SpecialBuildingType::CementThrower:       return "special_cement_thrower";
                case SpecialBuildingType::TunnelHub:           return "special_tunnel_hub";
                case SpecialBuildingType::EarthquakeGenerator: return "special_earthquake_generator";
                case SpecialBuildingType::BloodPool:           return "special_blood_pool";
                case SpecialBuildingType::DiseaseIncubator:    return "special_disease_incubator";
                case SpecialBuildingType::ShieldProjector:     return "special_shield_projector";
                case SpecialBuildingType::GroundAnchor:        return "special_ground_anchor";
                case SpecialBuildingType::WindTunnel:          return "special_wind_tunnel";
                case SpecialBuildingType::DivePerches:         return "special_dive_perches";
                case SpecialBuildingType::StinkGasVent:        return "special_stink_gas_vent";
                case SpecialBuildingType::AcidDepot:           return "special_acid_depot";
                case SpecialBuildingType::AmberMonolith:       return "special_amber_monolith";
                case SpecialBuildingType::FightingArena:       return "special_fighting_arena";
                case SpecialBuildingType::BatteringRamForge:   return "special_battering_ram_forge";
                case SpecialBuildingType::DeathZoneBanner:     return "special_death_zone_banner";
                case SpecialBuildingType::HuntHeadquarters:    return "special_hunt_headquarters";
                default:                                       return "special_unknown";
            }
        }
        default: return "unknown";
    }
}

std::string BuildingTexturePath(BugClass tribe, BuildingType type,
                                SpecialBuildingType special, BuildingDamageState damage) {
    std::string path = "assets/buildings/";
    path += BugClassSlug(tribe);
    path += '/';
    path += BuildingTypeSlug(type, special);
    switch (damage) {
        case BuildingDamageState::Damaged:     path += "_damaged";   break;
        case BuildingDamageState::HeavyDamage: path += "_damaged2";  break;
        case BuildingDamageState::Critical:    path += "_critical";  break;
        case BuildingDamageState::Destroyed:   path += "_destroyed"; break;
        default: break;
    }
    path += ".png";
    return path;
}

void BuildingTextureRegistry::Load(SDL_GPUDevice* device) {
    Clear();
    m_Device = device;

    const char* constructionPath = "assets/buildings/construction_sheet.png";
    if (std::filesystem::exists(constructionPath)) {
        m_ConstructionSheet = AssetManager::LoadTexture(constructionPath, TextureFilter::Nearest);
    }
    if (!m_ConstructionSheet || !m_ConstructionSheet->GetHandle()) {
        spdlog::warn("[BuildingTextures] Kein construction_sheet.png – generiere Platzhalter-Sprite-Sheet.");
        m_ConstructionSheet = GenerateConstructionSheet(device);
    }

    // Für jeden Tribe eine Mal-Vorlage generieren wenn main.png fehlt.
    // Die Datei enthält Rand, Grid und Eckmarkierungen als Orientierung.
    static const BugClass kAllTribes[] = {
        BugClass::BeesWasps, BugClass::ButterfliesMoths, BugClass::Snails,
        BugClass::Mantis,    BugClass::Spiders,           BugClass::Fireflies,
        BugClass::Ants,      BugClass::Termites,          BugClass::CentipedesWorms,
        BugClass::MosquitosTicks, BugClass::Woodlice,     BugClass::Dragonflies,
        BugClass::Bugs,      BugClass::Roaches,           BugClass::Beetles,
        BugClass::Scorpions,
    };
    int generated = 0;
    for (BugClass tribe : kAllTribes) {
        std::string mainPath = std::string("assets/buildings/")
                             + BugClassSlug(tribe) + "/main.png";

        // Vorlage generieren wenn:
        //  a) Datei fehlt, ODER
        //  b) Datei ist ein kleines PNG-Placeholder (<4 KB, kein echter Künstler-Asset)
        //     Echte Künstler-JPEGs (z.B. bees_wasps) beginnen nicht mit PNG-Signatur.
        bool needsTemplate = !std::filesystem::exists(mainPath);
        if (!needsTemplate && std::filesystem::file_size(mainPath) < 4096) {
            std::ifstream f(mainPath, std::ios::binary);
            uint8_t sig[4]{};
            f.read(reinterpret_cast<char*>(sig), 4);
            // PNG-Signatur: 89 50 4E 47
            if (sig[0] == 0x89 && sig[1] == 'P' && sig[2] == 'N' && sig[3] == 'G')
                needsTemplate = true;
        }

        if (needsTemplate) {
            std::filesystem::create_directories(
                std::filesystem::path(mainPath).parent_path());
            SaveBuildingTemplate(mainPath, tribe);
            ++generated;
            // Auch in src/Assets speichern damit der CMake-Copy-Step sie nicht überschreibt
            std::string srcPath = std::string("../src/Assets/buildings/")
                                + BugClassSlug(tribe) + "/main.png";
            if (std::filesystem::exists(std::filesystem::path(srcPath).parent_path()))
                SaveBuildingTemplate(srcPath, tribe);
        }
    }
    if (generated > 0)
        spdlog::info("[BuildingTextures] {} Mal-Vorlagen generiert in assets/buildings/*/main.png", generated);

    spdlog::info("[BuildingTextures] Bereit.");
}

void BuildingTextureRegistry::Clear() {
    m_Textures.clear();
    m_ConstructionSheet.reset();
    m_Device = nullptr;
}

std::shared_ptr<Texture> BuildingTextureRegistry::GetTexture(
    BugClass tribe, BuildingType type, SpecialBuildingType special,
    BuildingDamageState damage)
{
    if (!m_Device) return AssetManager::GetFallbackTexture();

    auto requestedPath = BuildingTexturePath(tribe, type, special, damage);
    auto it = m_Textures.find(requestedPath);
    if (it != m_Textures.end()) return it->second;

    return LoadOrGenerate(m_Device, requestedPath, tribe, type, special, damage);
}

std::shared_ptr<Texture> BuildingTextureRegistry::LoadOrGenerate(
    SDL_GPUDevice* device, const std::string& requestedPath,
    BugClass tribe, BuildingType type, SpecialBuildingType special,
    BuildingDamageState damage)
{
    // Kandidaten in Priorität: exakt → main+gleicher Schaden → exakt+gesund → main+gesund
    // Dadurch reicht es, nur main.png / main_damaged.png usw. bereitzustellen.
    std::vector<std::string> candidates;
    candidates.push_back(requestedPath);
    if (type != BuildingType::Main)
        candidates.push_back(BuildingTexturePath(tribe, BuildingType::Main, special, damage));
    if (damage != BuildingDamageState::Healthy) {
        candidates.push_back(BuildingTexturePath(tribe, type, special, BuildingDamageState::Healthy));
        candidates.push_back(BuildingTexturePath(tribe, BuildingType::Main, special, BuildingDamageState::Healthy));
    }

    for (const auto& path : candidates) {
        // Cache-Treffer
        auto cached = m_Textures.find(path);
        if (cached != m_Textures.end() && cached->second && cached->second->GetHandle()) {
            m_Textures[requestedPath] = cached->second;
            return cached->second;
        }
        // Datei laden
        if (std::filesystem::exists(path)) {
            auto tex = AssetManager::LoadTexture(path, TextureFilter::Nearest);
            if (tex && tex->GetHandle()) {
                m_Textures[path] = tex;
                m_Textures[requestedPath] = tex;
                if (path != requestedPath)
                    spdlog::info("[BuildingTextures] Fallback: {} -> {}", requestedPath, path);
                else
                    spdlog::info("[BuildingTextures] Geladen: {}", path);
                return tex;
            }
            spdlog::warn("[BuildingTextures] Laden fehlgeschlagen: {}", path);
        }
    }

    // Kein Bild gefunden → farbiger Platzhalter
    spdlog::warn("[BuildingTextures] Kein Bild, Platzhalter fuer: {}", requestedPath);
    auto generated = GeneratePlaceholder(device, tribe, type, special, damage);
    m_Textures[requestedPath] = generated;
    return generated;
}

std::shared_ptr<Texture> BuildingTextureRegistry::GeneratePlaceholder(
    SDL_GPUDevice* device, BugClass tribe, BuildingType type,
    SpecialBuildingType special, BuildingDamageState damage)
{
    constexpr uint32_t kSize = 64;
    std::vector<uint8_t> pixels(kSize * kSize * 4);

    glm::u8vec4 base = TribeBaseColor(tribe);
    uint32_t typeSeed = static_cast<uint32_t>(type) * 17u
                      + static_cast<uint32_t>(special) * 3u
                      + static_cast<uint32_t>(tribe) * 131u;
    base.r = static_cast<uint8_t>(glm::clamp<int>(base.r + (typeSeed % 40) - 20, 0, 255));
    base.g = static_cast<uint8_t>(glm::clamp<int>(base.g + ((typeSeed >> 4) % 30) - 15, 0, 255));
    base.b = static_cast<uint8_t>(glm::clamp<int>(base.b + ((typeSeed >> 8) % 30) - 15, 0, 255));

    if (damage == BuildingDamageState::Damaged) {
        base.r = static_cast<uint8_t>(glm::min(255, base.r + 35));
        base.g = static_cast<uint8_t>(base.g * 7 / 10);
        base.b = static_cast<uint8_t>(base.b * 7 / 10);
    } else if (damage == BuildingDamageState::Critical) {
        base.r = static_cast<uint8_t>(glm::min(255, base.r + 70));
        base.g = static_cast<uint8_t>(base.g / 2);
        base.b = static_cast<uint8_t>(base.b / 2);
    }

    for (uint32_t y = 0; y < kSize; ++y) {
        for (uint32_t x = 0; x < kSize; ++x) {
            const uint32_t idx = (y * kSize + x) * 4;
            const bool border = x < 2 || y < 2 || x >= kSize - 2 || y >= kSize - 2;
            const bool hatch = ((x / 4) + (y / 4)) % 2 == 0;
            glm::u8vec4 c = base;
            if (border) {
                c.r = static_cast<uint8_t>(c.r / 2);
                c.g = static_cast<uint8_t>(c.g / 2);
                c.b = static_cast<uint8_t>(c.b / 2);
            } else if (hatch && damage != BuildingDamageState::Healthy) {
                c.r = static_cast<uint8_t>(glm::min(255, c.r + 20));
            }
            pixels[idx + 0] = c.r;
            pixels[idx + 1] = c.g;
            pixels[idx + 2] = c.b;
            pixels[idx + 3] = 255;
        }
    }

    auto key = BuildingTexturePath(tribe, type, special, damage) + "_proc";
    return AssetManager::LoadTexture(key, pixels.data(), kSize, kSize, TextureFilter::Nearest);
}

std::shared_ptr<Texture> BuildingTextureRegistry::GenerateConstructionSheet(SDL_GPUDevice* device) {
    constexpr uint32_t kGrid = 8;
    constexpr uint32_t kCell = 64;
    constexpr uint32_t kSize = kGrid * kCell;
    std::vector<uint8_t> pixels(kSize * kSize * 4, 0);

    for (uint32_t frame = 0; frame < kGrid * kGrid; ++frame) {
        const uint32_t col = frame % kGrid;
        const uint32_t row = frame / kGrid;
        const float t = static_cast<float>(frame) / static_cast<float>(kGrid * kGrid - 1);
        for (uint32_t y = 0; y < kCell; ++y) {
            for (uint32_t x = 0; x < kCell; ++x) {
                const uint32_t px = col * kCell + x;
                const uint32_t py = row * kCell + y;
                const uint32_t idx = (py * kSize + px) * 4;
                const float edge = std::min({x / 8.f, y / 8.f, (kCell - x) / 8.f, (kCell - y) / 8.f});
                const float alpha = glm::clamp(edge, 0.f, 1.f) * (0.3f + 0.7f * t);
                pixels[idx + 0] = static_cast<uint8_t>(40 + 180 * t);
                pixels[idx + 1] = static_cast<uint8_t>(160 + 80 * (1.f - t));
                pixels[idx + 2] = static_cast<uint8_t>(50);
                pixels[idx + 3] = static_cast<uint8_t>(alpha * 255.f);
            }
        }
    }

    return AssetManager::LoadTexture("assets/buildings/construction_sheet_proc",
                                     pixels.data(), kSize, kSize, TextureFilter::Nearest);
}

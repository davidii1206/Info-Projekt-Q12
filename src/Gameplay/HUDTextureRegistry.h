/**
 * @file HUDTextureRegistry.h
 * @brief Lädt und verwaltet Pixel-Art-Texturen für das HUD.
 *
 * Der HUD-Designer legt seine PNG-Dateien in `src/Assets/UI/` ab und
 * registriert sie einmalig in `HUDTextures::Load()` (unten in dieser Datei).
 * Danach kann er überall im HUD-Code auf jede Textur per Name zugreifen
 * und sie mit ImGui zeichnen.
 *
 * -------------------------------------------------------------------------
 * ANLEITUNG FÜR DEN HUD-DESIGNER
 * -------------------------------------------------------------------------
 *
 * 1. PNG-Datei nach  src/Assets/UI/  kopieren, z.B. `icon_fleisch.png`
 *
 * 2. In `HUDTextures::Load()` (am Ende dieser Datei) eintragen:
 *      reg.Add(device, "fleisch",  "assets/UI/icon_fleisch.png");
 *      reg.Add(device, "pilze",    "assets/UI/icon_pilze.png");
 *      // … weitere Icons
 *
 * 3. Im HUD-Code (z.B. ResourceHUD.h) benutzen:
 *
 *      // Textur holen
 *      ImTextureID icon = HUDTextures::Get().GetImID("fleisch");
 *
 *      // Mit fester Größe zeichnen
 *      if (icon) ImGui::Image(icon, ImVec2(24, 24));
 *
 *      // Oder per DrawList an beliebiger Stelle
 *      ImVec2 pos = ImGui::GetCursorScreenPos();
 *      ImGui::GetWindowDrawList()->AddImage(
 *          icon, pos, ImVec2(pos.x + 24, pos.y + 24));
 *
 * -------------------------------------------------------------------------
 */

#pragma once
#include <SDL3/SDL_gpu.h>
#include <imgui.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "../Graphics/API/Texture.h"
#include <spdlog/spdlog.h>

// ---------------------------------------------------------------------------
// HUDTextureRegistry
// ---------------------------------------------------------------------------

/**
 * @class HUDTextureRegistry
 * @brief Lädt PNG-Texturen einmalig beim Spielstart und gibt sie als
 *        ImTextureID zurück.
 *
 * Pixel-Art-Texturen werden mit `TextureFilter::Nearest` geladen damit
 * sie scharf (nicht verwaschen) skaliert werden.
 */
class HUDTextureRegistry
{
public:
    HUDTextureRegistry() = default;

    // Nicht kopierbar – Texturen gehören diesem Objekt.
    HUDTextureRegistry(const HUDTextureRegistry&)            = delete;
    HUDTextureRegistry& operator=(const HUDTextureRegistry&) = delete;

    // ---------------------------------------------------------------------------
    // Textur laden
    // ---------------------------------------------------------------------------

    /**
     * @brief Lädt eine PNG-Datei und speichert sie unter `name`.
     *
     * Bereits vorhandene Einträge mit demselben Namen werden ersetzt.
     * Pixel-Art-Grafiken: filter = TextureFilter::Nearest (Standard hier).
     *
     * @param device   SDL_GPUDevice des Renderers  (ctx.renderer->GetDevice()).
     * @param name     Logischer Name, z.B. "fleisch", "icon_base".
     * @param filepath Pfad zur PNG-Datei, z.B. "assets/UI/icon_fleisch.png".
     * @param filter   TextureFilter::Nearest für Pixel-Art (Standard),
     *                 TextureFilter::Linear für glatte Bilder.
     * @return true wenn erfolgreich geladen.
     */
    bool Add(SDL_GPUDevice*     device,
             const std::string& name,
             const std::string& filepath,
             TextureFilter      filter = TextureFilter::Nearest)
    {
        auto tex = std::make_unique<Texture>(device, filepath, filter);
        if (!tex->GetHandle()) {
            spdlog::error("[HUDTextureRegistry] Konnte '{}' nicht laden: {}",
                          filepath, SDL_GetError());
            return false;
        }
        spdlog::info("[HUDTextureRegistry] Geladen: '{}' → '{}'  ({}x{})",
                     name, filepath, tex->GetWidth(), tex->GetHeight());
        m_Textures[name] = std::move(tex);
        return true;
    }

    // ---------------------------------------------------------------------------
    // Textur abrufen
    // ---------------------------------------------------------------------------

    /**
     * @brief Gibt die `ImTextureID` für einen registrierten Namen zurück.
     *
     * Gibt `nullptr` zurück wenn der Name unbekannt ist — ImGui::Image()
     * mit nullptr ist sicher (zeichnet nichts).
     *
     * @param name  Der beim Add() verwendete Name.
     * @return ImTextureID (cast von SDL_GPUTexture*), oder nullptr.
     */
    ImTextureID GetImID(const std::string& name) const
    {
        auto it = m_Textures.find(name);
        if (it == m_Textures.end()) {
            spdlog::warn("[HUDTextureRegistry] Textur '{}' nicht gefunden.", name);
            return static_cast<ImTextureID>(0);
        }
        return reinterpret_cast<ImTextureID>(it->second->GetHandle());
    }

    /**
     * @brief Gibt Breite und Höhe einer Textur zurück.
     *
     * Nützlich um Texturen in Originalgröße zu zeichnen:
     * @code
     *   auto [w, h] = HUDTextures::Get().GetSize("fleisch");
     *   ImGui::Image(icon, ImVec2((float)w, (float)h));
     * @endcode
     *
     * @return {0, 0} wenn der Name unbekannt ist.
     */
    std::pair<uint32_t, uint32_t> GetSize(const std::string& name) const
    {
        auto it = m_Textures.find(name);
        if (it == m_Textures.end()) return {0, 0};
        return {it->second->GetWidth(), it->second->GetHeight()};
    }

    /**
     * @brief Gibt true zurück wenn die Textur erfolgreich geladen wurde.
     */
    bool Has(const std::string& name) const
    {
        return m_Textures.count(name) > 0;
    }

    /**
     * @brief Gibt alle registrierten Namen zurück (für Debug-Zwecke).
     */
    std::vector<std::string> AllNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_Textures.size());
        for (const auto& [k, _] : m_Textures)
            names.push_back(k);
        return names;
    }

    /// Gibt alle Texturen frei (automatisch beim Destruktor).
    void Clear() { m_Textures.clear(); }

private:
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
};

// ---------------------------------------------------------------------------
// HUDTextures – globale Singleton-Instanz
// ---------------------------------------------------------------------------

/**
 * @namespace HUDTextures
 * @brief Globaler Zugriff auf die HUDTextureRegistry.
 *
 * `HUDTextures::Get()` gibt die einzige Instanz zurück.
 * `HUDTextures::Load()` wird einmalig beim Spielstart aufgerufen.
 */
namespace HUDTextures
{
    /// Gibt die globale Registry-Instanz zurück.
    inline HUDTextureRegistry& Get()
    {
        static HUDTextureRegistry instance;
        return instance;
    }

    /**
     * @brief Lädt alle HUD-Texturen.
     *
     * =========================================================
     *  HIER TRAGT DER HUD-DESIGNER SEINE TEXTUREN EIN
     * =========================================================
     *
     * Format:
     *   reg.Add(device, "name", "assets/UI/dateiname.png");
     *
     * TextureFilter::Nearest  = scharf/pixelig  (Pixel-Art)
     * TextureFilter::Linear   = weich/geglättet (Fotos, Icons)
     *
     * @param device  ctx.renderer->GetDevice()  aus GameScene::OnEnter().
     */
    inline void Load(SDL_GPUDevice* device)
    {
        auto& reg = Get();

        // ------------------------------------------------------------------
        // Ressourcen-Icons
        // ------------------------------------------------------------------
        reg.Add(device, "icon_pilze",    "assets/UI/icon_pilze.png");
        reg.Add(device, "icon_beeren",   "assets/UI/icon_beeren.png");
        reg.Add(device, "icon_nektar",   "assets/UI/icon_nektar.png");
        reg.Add(device, "icon_samen",    "assets/UI/icon_samen.png");
        reg.Add(device, "icon_insekten", "assets/UI/icon_insekten.png");
        reg.Add(device, "icon_fleisch",  "assets/UI/icon_fleisch.png");

        // ------------------------------------------------------------------
        // Karten-/Minimap-Elemente
        // ------------------------------------------------------------------
        reg.Add(device, "map_bg",        "assets/UI/map_background.png");
        reg.Add(device, "map_fog",       "assets/UI/map_fog.png");
        reg.Add(device, "marker_base",   "assets/UI/marker_base.png");
        reg.Add(device, "marker_player", "assets/UI/marker_player.png");

        // ------------------------------------------------------------------
        // Weitere HUD-Elemente – bei Bedarf ergänzen
        // ------------------------------------------------------------------
        // reg.Add(device, "hud_frame",  "assets/UI/hud_frame.png");
        // reg.Add(device, "btn_karte",  "assets/UI/btn_karte.png");

        spdlog::info("[HUDTextures] {} Textur(en) registriert.",
                     reg.AllNames().size());
    }

    /// Gibt alle Texturen frei – in GameScene::OnExit() aufrufen.
    inline void Unload()
    {
        Get().Clear();
        spdlog::info("[HUDTextures] Alle HUD-Texturen freigegeben.");
    }

} // namespace HUDTextures

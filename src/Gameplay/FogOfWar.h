/**
 * @file FogOfWar.h
 * @brief Grid-based Fog of War (Discover Map) system.
 *
 * The map is divided into a uniform grid of cells. Each cell has two states:
 *  - HIDDEN   – never visited, shown as dark overlay
 *  - REVEALED – a unit has been here, permanently visible
 *
 * The FogGrid is a plain value object that can live on the server and be
 * serialised to clients (or kept locally for a single-player / host setup).
 *
 * Usage (server, inside GameScene):
 * @code
 *   // Init once in OnEnter()
 *   m_Fog.Init(worldMin, worldMax, cellSize);
 *
 *   // Call every fixed tick in FixedUpdate()
 *   FogOfWarSystem::Update(m_Fog, ctx.serverRegistry);
 * @endcode
 *
 * Usage (client UI, inside UIUpdate()):
 * @code
 *   FogOfWarSystem::DrawOverlay(m_Fog, mapOriginScreen, mapSizeScreen);
 * @endcode
 */

#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include "Components.h"

// ---------------------------------------------------------------------------
// FogGrid – pure data, no ECS dependency
// ---------------------------------------------------------------------------

/**
 * @struct FogGrid
 * @brief Flat 2D bit-grid tracking which cells have been revealed.
 *
 * Coordinates: X = right, Z = depth (Y is height, ignored for fog purposes).
 */
struct FogGrid
{
    glm::vec3 worldMin{-50.f, 0.f, -50.f}; ///< Bottom-left corner of the grid.
    glm::vec3 worldMax{ 50.f, 0.f,  50.f}; ///< Top-right corner of the grid.
    float     cellSize = 2.f;               ///< World-units per cell edge.

    int cellsX = 0; ///< Number of cells along X.
    int cellsZ = 0; ///< Number of cells along Z.

    /// Flat array: revealed[z * cellsX + x] == true → cell visible.
    std::vector<bool> revealed;

    /**
     * @brief Initialises the grid. Call once before use.
     * @param min      World-space minimum (X and Z used).
     * @param max      World-space maximum (X and Z used).
     * @param cell     World-units per cell edge.
     */
    void Init(glm::vec3 min = {-50.f,0.f,-50.f},
              glm::vec3 max = { 50.f,0.f, 50.f},
              float     cell = 2.f)
    {
        worldMin = min;
        worldMax = max;
        cellSize = cell;

        cellsX = static_cast<int>(std::ceil((max.x - min.x) / cell));
        cellsZ = static_cast<int>(std::ceil((max.z - min.z) / cell));

        revealed.assign(static_cast<size_t>(cellsX * cellsZ), false);
    }

    /// Converts a world position to a grid cell index (clamped).
    void WorldToCell(const glm::vec3& pos, int& outX, int& outZ) const
    {
        outX = static_cast<int>((pos.x - worldMin.x) / cellSize);
        outZ = static_cast<int>((pos.z - worldMin.z) / cellSize);
        outX = std::max(0, std::min(cellsX - 1, outX));
        outZ = std::max(0, std::min(cellsZ - 1, outZ));
    }

    /// Returns true if the given grid index is inside the grid bounds.
    bool InBounds(int cx, int cz) const
    {
        return cx >= 0 && cx < cellsX && cz >= 0 && cz < cellsZ;
    }

    /// Reveals all cells within `radius` world-units of `center`.
    void Reveal(const glm::vec3& center, float radius)
    {
        int cx, cz;
        WorldToCell(center, cx, cz);

        int cellRadius = static_cast<int>(std::ceil(radius / cellSize));
        for (int dz = -cellRadius; dz <= cellRadius; ++dz)
        for (int dx = -cellRadius; dx <= cellRadius; ++dx)
        {
            // Circle check in world units
            float wx = (cx + dx + 0.5f) * cellSize + worldMin.x;
            float wz = (cz + dz + 0.5f) * cellSize + worldMin.z;
            float dist2 = (wx - center.x) * (wx - center.x)
                        + (wz - center.z) * (wz - center.z);
            if (dist2 > radius * radius) continue;

            int nx = cx + dx;
            int nz = cz + dz;
            if (InBounds(nx, nz))
                revealed[static_cast<size_t>(nz * cellsX + nx)] = true;
        }
    }

    bool IsRevealed(int cx, int cz) const
    {
        if (!InBounds(cx, cz)) return false;
        return revealed[static_cast<size_t>(cz * cellsX + cx)];
    }

    /// Resets all cells to hidden (useful on map restart).
    void Reset() { revealed.assign(revealed.size(), false); }

    bool IsInitialised() const { return !revealed.empty(); }
};

// ---------------------------------------------------------------------------
// FogOfWarSystem
// ---------------------------------------------------------------------------

/**
 * @namespace FogOfWarSystem
 * @brief ECS system that updates the FogGrid based on unit positions and
 *        renders the fog overlay via ImGui.
 */
namespace FogOfWarSystem
{
    /// Sichtweite für benutzersteuerbare Einheiten (Welt-Einheiten).
    constexpr float PLAYER_SIGHT_RADIUS   = 12.f;
    /// Sichtweite für Sammel- und AI-Einheiten.
    constexpr float COLLECTOR_SIGHT_RADIUS = 8.f;

    /**
     * @brief Reveals fog around every unit that has a TransformComponent.
     *
     * Players use PLAYER_SIGHT_RADIUS, CollectorComponent units use
     * COLLECTOR_SIGHT_RADIUS.
     *
     * Call every server fixed-tick.
     *
     * @param fog      The grid to update.
     * @param registry Server or client registry.
     */
    inline void Update(FogGrid& fog, entt::registry& registry)
    {
        if (!fog.IsInitialised()) return;

        // Players
        {
            auto view = registry.view<TransformComponent, PlayerComponent>();
            for (auto e : view) {
                auto& p = view.get<PlayerComponent>(e);
                if (p.cameraMode == CameraMode::Commander || p.cameraMode == CameraMode::Building || p.cameraMode == CameraMode::FreeFly)
                    continue;

                fog.Reveal(view.get<TransformComponent>(e).position,
                           PLAYER_SIGHT_RADIUS);
            }
        }

        // Collector units
        {
            auto view = registry.view<TransformComponent, CollectorComponent>();
            for (auto e : view)
                fog.Reveal(view.get<TransformComponent>(e).position,
                           COLLECTOR_SIGHT_RADIUS);
        }
    }

    /**
     * @brief Draws the fog of war as a semi-transparent ImGui overlay.
     *
     * Renders an ImGui window sized to `mapSizePx` at `mapOriginPx` and
     * fills unrevealed cells with a dark rectangle.
     *
     * Call from UIUpdate().  The overlay is purely visual — no interaction.
     *
     * @param fog          The fog grid to visualise.
     * @param mapOriginPx  Top-left pixel of the minimap / full-screen map in
     *                     screen space.  Pass {0,0} for a full-screen overlay.
     * @param mapSizePx    Width and height of the map area in pixels.
     * @param fogAlpha     Opacity of hidden cells (0 = transparent, 1 = black).
     */
    inline void DrawOverlay(const FogGrid& fog,
                            ImVec2         mapOriginPx,
                            ImVec2         mapSizePx,
                            float          fogAlpha = 0.85f)
    {
        if (!fog.IsInitialised()) return;

        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoDecoration      |
            ImGuiWindowFlags_NoInputs          |
            ImGuiWindowFlags_NoNav             |
            ImGuiWindowFlags_NoMove            |
            ImGuiWindowFlags_NoSavedSettings   |
            ImGuiWindowFlags_NoFocusOnAppearing|
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::SetNextWindowPos(mapOriginPx, ImGuiCond_Always);
        ImGui::SetNextWindowSize(mapSizePx,  ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.f); // transparent window, cells drawn manually
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

        if (ImGui::Begin("##FogOfWar", nullptr, kFlags))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32 fogColor = IM_COL32(0, 0, 0,
                static_cast<int>(fogAlpha * 255.f));

            const float cellW = mapSizePx.x / static_cast<float>(fog.cellsX);
            const float cellH = mapSizePx.y / static_cast<float>(fog.cellsZ);

            for (int z = 0; z < fog.cellsZ; ++z)
            for (int x = 0; x < fog.cellsX; ++x)
            {
                if (fog.IsRevealed(x, z)) continue;

                ImVec2 tl(mapOriginPx.x + x * cellW,
                          mapOriginPx.y + z * cellH);
                ImVec2 br(tl.x + cellW + 1.f,  // +1 prevents hairline gaps
                          tl.y + cellH + 1.f);
                dl->AddRectFilled(tl, br, fogColor);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

} // namespace FogOfWarSystem

/**
 * @file TerritorySystem.h
 * @brief Zone-based territory system with coloured ownership marking.
 *
 * Each territory is an axis-aligned rectangular zone on the map.
 * A zone is captured when a team has had more units inside it than any
 * other team for `captureTime` seconds.
 *
 * Components
 * ----------
 *  TerritoryComponent  –  attach to a "zone" entity (TransformComponent
 *                          defines its centre, TerritoryComponent its size
 *                          and ownership state).
 *
 * Systems
 * -------
 *  TerritorySystem::Update()  – server fixed-tick, recalculates unit counts
 *                                and advances capture progress.
 *  TerritorySystem::DrawOverlay() – client UIUpdate(), draws coloured zone
 *                                    rectangles via ImGui DrawList.
 *
 * Usage
 * -----
 * @code
 *   // OnEnter() – server only
 *   TerritorySystem::SpawnZones(ctx.serverRegistry);
 *
 *   // FixedUpdate() – server only
 *   TerritorySystem::Update(ctx.serverRegistry, dt);
 *
 *   // UIUpdate() – host only (mirror to clients via packets later)
 *   TerritorySystem::DrawOverlay(ctx.serverRegistry,
 *                                mapOriginPx, mapSizePx,
 *                                worldMin, worldMax);
 * @endcode
 */

#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <array>
#include <cstring>
#include "Components.h"

// ---------------------------------------------------------------------------
// Team colour palette  (up to 8 teams)
// ---------------------------------------------------------------------------

namespace TerritoryColors
{
    /// RGBA colours used for team-owned territory overlays.
    inline ImVec4 ForTeam(uint32_t teamId, float alpha = 0.35f)
    {
        static const float palette[][3] = {
            {0.20f, 0.60f, 1.00f},  // 0 – blue
            {1.00f, 0.25f, 0.25f},  // 1 – red
            {0.20f, 0.85f, 0.30f},  // 2 – green
            {1.00f, 0.80f, 0.10f},  // 3 – yellow
            {0.80f, 0.20f, 0.90f},  // 4 – purple
            {1.00f, 0.50f, 0.10f},  // 5 – orange
            {0.10f, 0.90f, 0.90f},  // 6 – cyan
            {0.90f, 0.90f, 0.90f},  // 7 – white
        };
        const uint32_t idx = teamId % 8;
        return ImVec4(palette[idx][0], palette[idx][1], palette[idx][2], alpha);
    }

    inline ImU32 ForTeamU32(uint32_t teamId, float alpha = 0.35f)
    {
        ImVec4 c = ForTeam(teamId, alpha);
        return IM_COL32(
            static_cast<int>(c.x * 255),
            static_cast<int>(c.y * 255),
            static_cast<int>(c.z * 255),
            static_cast<int>(c.w * 255));
    }
} // namespace TerritoryColors

// ---------------------------------------------------------------------------
// TerritoryComponent
// ---------------------------------------------------------------------------

/// Sentinel: no team owns this zone.
static constexpr uint32_t TEAM_NONE = 0xFFFF'FFFFu;

/**
 * @struct TerritoryComponent
 * @brief Marks an entity as a capturable territory zone.
 *
 * Attach alongside TransformComponent (centre position).
 * The zone is a 2D rectangle in the XZ plane:
 *   [pos.x - halfW, pos.x + halfW] × [pos.z - halfD, pos.z + halfD]
 */
struct TerritoryComponent
{
    char     name[32]       = "Zone";   ///< Display name shown in overlay.
    float    halfW          = 8.f;      ///< Half-width along X.
    float    halfD          = 8.f;      ///< Half-depth along Z.
    float    captureTime    = 10.f;     ///< Seconds of dominance needed to capture.

    uint32_t ownerTeam      = TEAM_NONE;///< Currently owning team (TEAM_NONE if neutral).
    uint32_t contestedBy    = TEAM_NONE;///< Team currently ahead in capture progress.
    float    captureProgress = 0.f;     ///< 0–captureTime seconds of progress.

    TerritoryComponent() = default;
    TerritoryComponent(const char* n, float hw, float hd, float ct = 10.f)
        : halfW(hw), halfD(hd), captureTime(ct)
    {
        std::strncpy(name, n, sizeof(name) - 1);
    }
};

// ---------------------------------------------------------------------------
// TerritorySystem
// ---------------------------------------------------------------------------

namespace TerritorySystem
{
    // -----------------------------------------------------------------------
    // SpawnZones – define your map's territory layout here
    // -----------------------------------------------------------------------

    /**
     * @brief Spawns the default set of territory zones for the current map.
     *
     * Edit the zone list below to match your actual map geometry.
     * Call once from GameScene::OnEnter() on the server.
     *
     * @param registry  Authoritative server registry.
     */
    inline void SpawnZones(entt::registry& registry)
    {
        struct ZoneDef {
            const char* name;
            glm::vec3   center;
            float       halfW, halfD;
            float       captureTime;
        };

        static const ZoneDef defs[] = {
            { "Nord-Lager",  { 0.f, 0.f,  20.f}, 8.f, 8.f, 12.f },
            { "Sued-Lager",  { 0.f, 0.f, -20.f}, 8.f, 8.f, 12.f },
            { "Mitte",       { 0.f, 0.f,   0.f}, 6.f, 6.f,  8.f },
            { "Ost-Posten",  {20.f, 0.f,   0.f}, 5.f, 5.f, 10.f },
            { "West-Posten", {-20.f,0.f,   0.f}, 5.f, 5.f, 10.f },
        };

        for (const auto& d : defs)
        {
            auto e = registry.create();
            registry.emplace<TransformComponent>(e, d.center);
            registry.emplace<TerritoryComponent>(e,
                d.name, d.halfW, d.halfD, d.captureTime);
        }

        spdlog::info("[TerritorySystem] Spawned {} territory zones.",
                     std::size(defs));
    }

    // -----------------------------------------------------------------------
    // Update – server fixed-tick
    // -----------------------------------------------------------------------

    /**
     * @brief Recalculates unit presence inside each zone and advances capture.
     *
     * Logic per zone each tick:
     *  1. Count units per team inside the zone bounds.
     *  2. Find the leading team (most units).
     *  3. If the leading team differs from the current owner, advance
     *     captureProgress.  If no team is dominant (tie / empty), decay.
     *  4. When captureProgress >= captureTime the zone changes owner.
     *
     * @param registry  Authoritative server registry.
     * @param dt        Fixed delta time in seconds.
     */
    inline void Update(entt::registry& registry, float dt)
    {
        // Collect all units with positions and team affiliation
        struct UnitInfo { glm::vec3 pos; uint32_t team; };
        std::vector<UnitInfo> units;

        {
            auto view = registry.view<TransformComponent, PlayerComponent>();
            for (auto e : view)
                units.push_back({
                    view.get<TransformComponent>(e).position,
                    view.get<PlayerComponent>(e).playerId % 2  // 2-team split by parity; replace with TeamComponent later
                });
        }

        // Tick every territory zone
        auto zoneView = registry.view<TransformComponent, TerritoryComponent>();
        for (auto zoneEntity : zoneView)
        {
            const auto& tf  = zoneView.get<TransformComponent>(zoneEntity);
            auto&       ter = zoneView.get<TerritoryComponent>(zoneEntity);

            // Count units per team inside zone bounds (XZ rectangle)
            std::array<int, 8> teamCount = {};
            int totalInside = 0;

            for (const auto& u : units)
            {
                bool inside =
                    u.pos.x >= tf.position.x - ter.halfW &&
                    u.pos.x <= tf.position.x + ter.halfW &&
                    u.pos.z >= tf.position.z - ter.halfD &&
                    u.pos.z <= tf.position.z + ter.halfD;

                if (!inside) continue;
                teamCount[u.team % 8]++;
                ++totalInside;
            }

            // Find dominant team
            uint32_t leadTeam  = TEAM_NONE;
            int      leadCount = 0;
            bool     contested = false;

            for (uint32_t t = 0; t < 8; ++t)
            {
                if (teamCount[t] > leadCount) {
                    leadCount = teamCount[t];
                    leadTeam  = t;
                    contested = false;
                } else if (teamCount[t] == leadCount && leadCount > 0) {
                    contested = true;
                }
            }

            if (leadCount == 0 || contested)
            {
                // No dominant team → decay progress toward 0
                ter.captureProgress = std::max(0.f,
                    ter.captureProgress - dt * 0.5f);
                ter.contestedBy = TEAM_NONE;
                continue;
            }

            // A team is dominant
            if (leadTeam == ter.ownerTeam)
            {
                // Already owned by dominant team – no change needed
                ter.captureProgress = ter.captureTime; // keep at max
                ter.contestedBy     = TEAM_NONE;
                continue;
            }

            // Contested by a different team
            ter.contestedBy = leadTeam;

            if (ter.ownerTeam == TEAM_NONE)
            {
                // Neutral zone: just accumulate
                ter.captureProgress += dt;
            }
            else
            {
                // Owned zone: attackers first drain the owner's progress to 0,
                // then the zone becomes neutral, then re-captured.
                ter.captureProgress -= dt;
                if (ter.captureProgress <= 0.f)
                {
                    spdlog::info("[TerritorySystem] Zone '{}' neutralised (was team {}).",
                                 ter.name, ter.ownerTeam);
                    ter.ownerTeam       = TEAM_NONE;
                    ter.captureProgress = 0.f;
                }
                continue;
            }

            if (ter.captureProgress >= ter.captureTime)
            {
                ter.captureProgress = ter.captureTime;
                ter.ownerTeam       = leadTeam;
                ter.contestedBy     = TEAM_NONE;
                spdlog::info("[TerritorySystem] Zone '{}' captured by team {}.",
                             ter.name, leadTeam);
            }
        }
    }

    // -----------------------------------------------------------------------
    // DrawOverlay – ImGui client visualisation
    // -----------------------------------------------------------------------

    /**
     * @brief Draws territory zones as coloured rectangles on a minimap overlay.
     *
     * Maps world XZ coordinates linearly onto the pixel rectangle defined by
     * [mapOriginPx, mapOriginPx + mapSizePx].
     *
     * @param registry     Registry containing TerritoryComponent entities.
     * @param mapOriginPx  Top-left pixel of the map area.
     * @param mapSizePx    Width / height of the map area in pixels.
     * @param worldMin     World-space XZ minimum that maps to mapOriginPx.
     * @param worldMax     World-space XZ maximum that maps to mapOriginPx + mapSizePx.
     */
    inline void DrawOverlay(entt::registry& registry,
                            ImVec2          mapOriginPx,
                            ImVec2          mapSizePx,
                            glm::vec2       worldMin = {-50.f, -50.f},
                            glm::vec2       worldMax = { 50.f,  50.f})
    {
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
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

        if (ImGui::Begin("##TerritoryOverlay", nullptr, kFlags))
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Helper: world XZ → screen pixel
            auto ToScreen = [&](float wx, float wz) -> ImVec2 {
                float nx = (wx - worldMin.x) / (worldMax.x - worldMin.x);
                float nz = (wz - worldMin.y) / (worldMax.y - worldMin.y);
                return ImVec2(mapOriginPx.x + nx * mapSizePx.x,
                              mapOriginPx.y + nz * mapSizePx.y);
            };

            auto zoneView = registry.view<TransformComponent, TerritoryComponent>();
            for (auto e : zoneView)
            {
                const auto& tf  = zoneView.get<TransformComponent>(e);
                const auto& ter = zoneView.get<TerritoryComponent>(e);

                ImVec2 tl = ToScreen(tf.position.x - ter.halfW,
                                     tf.position.z - ter.halfD);
                ImVec2 br = ToScreen(tf.position.x + ter.halfW,
                                     tf.position.z + ter.halfD);

                // Fill colour: owner colour or grey if neutral
                ImU32 fillCol;
                if (ter.ownerTeam != TEAM_NONE)
                    fillCol = TerritoryColors::ForTeamU32(ter.ownerTeam, 0.35f);
                else
                    fillCol = IM_COL32(180, 180, 180, 60);

                dl->AddRectFilled(tl, br, fillCol, 4.f);

                // Contested progress bar along bottom edge of zone rect
                if (ter.contestedBy != TEAM_NONE && ter.captureTime > 0.f)
                {
                    float pct = ter.captureProgress / ter.captureTime;
                    ImVec2 barTL(tl.x, br.y - 4.f);
                    ImVec2 barBR(tl.x + (br.x - tl.x) * pct, br.y);
                    dl->AddRectFilled(barTL, barBR,
                        TerritoryColors::ForTeamU32(ter.contestedBy, 0.9f));
                }

                // Border: brighter when contested
                ImU32 borderCol = (ter.contestedBy != TEAM_NONE)
                    ? TerritoryColors::ForTeamU32(ter.contestedBy, 1.f)
                    : IM_COL32(255, 255, 255, 120);
                dl->AddRect(tl, br, borderCol, 4.f, 0, 1.5f);

                // Zone name label centered
                ImVec2 labelPos(
                    (tl.x + br.x) * 0.5f - ImGui::CalcTextSize(ter.name).x * 0.5f,
                    (tl.y + br.y) * 0.5f - 6.f);
                dl->AddText(labelPos, IM_COL32(255, 255, 255, 200), ter.name);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

} // namespace TerritorySystem

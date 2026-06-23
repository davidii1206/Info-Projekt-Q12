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
 */

#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <array>
#include <cstring>
#include <cstdio>
#include "Components.h"
#include "../Core/WorldManager.h"

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

    /// @brief Like ForTeam but returns a packed ImU32 colour suitable for ImDrawList.
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

    uint16_t worldTerritoryId = 0xFFFF; ///< Index into WorldManager::GetTerrains(), or 0xFFFF.
    uint32_t ownerTeam      = TEAM_NONE;///< Currently owning team (TEAM_NONE if neutral).
    uint32_t contestedBy    = TEAM_NONE;///< Team currently ahead in capture progress.
    float    captureProgress = 0.f;     ///< 0–captureTime seconds of progress.

    TerritoryComponent() = default;
    /// @brief Constructs a named zone with explicit half-extents and capture time.
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
     * @brief Returns a short display name for the given BugClass faction.
     */
    inline const char* FactionName(BugClass bc)
    {
        switch (bc) {
            case BugClass::BeesWasps:        return "Stecher-Allianz";
            case BugClass::ButterfliesMoths: return "Lepidoptera";
            case BugClass::Snails:           return "Panzer-Konsortium";
            case BugClass::Mantis:           return "Assassinen-Orden";
            case BugClass::Spiders:          return "Seiden-Syndikat";
            case BugClass::Fireflies:        return "Licht-Kollektiv";
            case BugClass::Ants:             return "Die Legion";
            case BugClass::Termites:         return "Baumeister";
            case BugClass::CentipedesWorms:  return "Unterwelt-Gilde";
            case BugClass::MosquitosTicks:   return "Die Plage";
            case BugClass::Woodlice:         return "Die Phalanx";
            case BugClass::Dragonflies:      return "Apex-Jaeger";
            case BugClass::Bugs:             return "Chem-Kartell";
            case BugClass::Roaches:          return "Unsterbliche";
            case BugClass::Beetles:          return "Gladiatoren";
            case BugClass::Scorpions:        return "Wuesten-Nomaden";
            default:                         return "Unbekannt";
        }
    }

    /**
     * @brief Spawns one territory zone per WorldManager territory, derived
     *        from the tile grid.
     *
     * Each zone is centered on the territory's pre-computed spawn tile
     * (the nearest buildable plateau tile to the Voronoi site). Zone size
     * and capture time are fixed; swap for per-territory values once
     * gameplay data drives them.
     *
     * @param registry  Authoritative server registry.
     * @param world     Generated WorldManager (must already have Generate() called).
     */
    inline void SpawnZones(entt::registry& registry, const WorldManager& world)
    {
        const auto& terrains = world.GetTerrains();
        const auto& cfg      = world.GetConfig();

        // Capture zone half-size in world units. Roughly 6% of the world
        // extent so zones scale naturally with map size.
        const float halfSize    = cfg.worldExtent * 0.06f;
        const float captureTime = 15.f;

        int spawned = 0;
        for (const auto& td : terrains)
        {
            // Verify the spawn tile is actually buildable; skip if not.
            int tx, tz;
            world.WorldToTile(td.spawnPoint.x, td.spawnPoint.y, tx, tz);
            const TerrainTile& tile = world.GetTile(tx, tz);
            if (!tile.buildable) continue;

            float worldY = world.TierToWorldHeight(tile.tier);
            glm::vec3 center(td.spawnPoint.x, worldY, td.spawnPoint.y);

            auto e = registry.create();
            registry.emplace<TransformComponent>(e, center);

            TerritoryComponent ter(FactionName(td.bugClass), halfSize, halfSize, captureTime);
            ter.worldTerritoryId = (uint16_t)td.id;
            registry.emplace<TerritoryComponent>(e, ter);
            ++spawned;
        }

        spdlog::info("[TerritorySystem] Spawned {} territory zones (halfSize={:.1f}).",
                     spawned, halfSize);
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
                    view.get<PlayerComponent>(e).playerId  // FFA: each player is their own team
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

} // namespace TerritorySystem

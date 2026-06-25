/**
 * @file ResourceHUD.h
 * @brief ImGui-Overlay that displays the current resource stockpile of the
 *        local player's base.
 *
 * Usage (inside GameScene::UIUpdate, after the player ID is assigned):
 * @code
 *   ResourceHUD::Draw(ctx.serverRegistry, m_MyPlayerId);
 * @endcode
 *
 * The overlay reads all BaseComponent + ResourceInventory entities that
 * match the given teamId and renders a compact, non-interactive ImGui
 * window in the top-right corner of the screen.
 */

#pragma once
#include <entt/entt.hpp>
#include <imgui.h>
#include "ResourceTypes.h"
#include "Components.h"

namespace ResourceHUD
{

/**
 * @brief Renders the resource stockpile HUD from a directly-supplied inventory.
 *
 * Used by the joined-client path, which mirrors the host's per-base inventory
 * via INVENTORY_UPDATE packets into a local map. The host can keep calling the
 * registry overload below; both share the same drawing code.
 */
inline void DrawInventory(const ResourceInventory& inv, int activeMeatDrops = 0)
{
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration      |
        ImGuiWindowFlags_AlwaysAutoResize  |
        ImGuiWindowFlags_NoSavedSettings   |
        ImGuiWindowFlags_NoFocusOnAppearing|
        ImGuiWindowFlags_NoNav             |
        ImGuiWindowFlags_NoMove;

    const float PAD = 10.f;
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos(io.DisplaySize.x - PAD, PAD);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::SetNextWindowBgAlpha(0.72f);

    if (ImGui::Begin("##ResourceHUD", nullptr, kFlags))
    {
        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.3f, 1.f), "Ressourcen");
        ImGui::Separator();

        auto Row = [](const char* emoji, const char* label, int amount,
                      ImVec4 colour = ImVec4(1,1,1,1))
        {
            ImGui::TextColored(colour, "%s %-10s", emoji, label);
            ImGui::SameLine(130.f);
            ImGui::Text("%d", amount);
        };

        // Universal upgrade currency — listed first so it stands out.
        Row(u8"\U0001FAB5", "Holz",     inv.holz,
            ImVec4(0.78f, 0.55f, 0.30f, 1.f));
        ImGui::Separator();
        Row(u8"\U0001F344", "Pilze",    inv.pilze);
        Row(u8"\U0001F353", "Beeren",   inv.beeren);
        Row(u8"\U0001F33C", "Nektar",   inv.nektar);
        Row(u8"\U0001F33E", "Samen",    inv.samen);
        Row(u8"\U0001F41B", "Insekten", inv.insekten);
        Row(u8"\U0001F969", "Fleisch",  inv.fleisch,
            ImVec4(1.f, 0.5f, 0.5f, 1.f));

        if (activeMeatDrops > 0) {
            ImGui::Spacing();
            ImGui::TextDisabled("  (%d Drop%s in der Welt)",
                                activeMeatDrops,
                                activeMeatDrops == 1 ? "" : "s");
        }
    }
    ImGui::End();
}

/**
 * @brief Renders the resource stockpile HUD for a given team.
 *
 * Finds the base entity whose BaseComponent::teamId matches `teamId`,
 * then displays its ResourceInventory as an ImGui overlay window.
 * If no matching base is found the window is still shown with all zeros.
 *
 * @param registry  Registry to query (typically ctx.serverRegistry on
 *                  the host, or a local mirror on pure clients).
 * @param teamId    Which team's stockpile to display.
 */
inline void Draw(entt::registry& registry, uint32_t teamId)
{
    // --- Find the base inventory for this team ---
    const ResourceInventory* inv = nullptr;
    {
        auto view = registry.view<BaseComponent, ResourceInventory>();
        for (auto entity : view) {
            if (view.get<BaseComponent>(entity).teamId == teamId) {
                inv = &view.get<ResourceInventory>(entity);
                break;
            }
        }
    }
    if (!inv) {
        // Also check new BuildingComponent with ResourceInventory
        auto bldView = registry.view<BuildingComponent, ResourceInventory>();
        for (auto entity : bldView) {
            if (bldView.get<BuildingComponent>(entity).teamId == teamId &&
                !bldView.get<BuildingComponent>(entity).destroyed) {
                inv = &bldView.get<ResourceInventory>(entity);
                break;
            }
        }
    }

    // Fallback: empty inventory if no base entity exists yet
    ResourceInventory empty{};
    if (!inv) inv = &empty;

    // --- Count active (non-depleted) Fleisch drops in the world ---
    int activeMeatDrops = 0;
    {
        auto dropView = registry.view<ResourceComponent>();
        for (auto e : dropView) {
            const auto& rc = dropView.get<ResourceComponent>(e);
            if (rc.type == ResourceType::Fleisch && !rc.permanent && !rc.depleted)
                ++activeMeatDrops;
        }
    }

    // Delegate to the inventory-only renderer.
    DrawInventory(*inv, activeMeatDrops);
}

} // namespace ResourceHUD

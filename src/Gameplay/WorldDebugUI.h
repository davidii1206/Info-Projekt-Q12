/**
 * @file WorldDebugUI.h
 * @brief Provides ImGui-based debugging tools for visualizing and manipulating the ECS world.
 */

#pragma once
#include <imgui.h>
#include <entt/entt.hpp>
#include "World.h"
#include "Components.h"

/**
 * @namespace WorldDebugUI
 * @brief Contains functions for drawing the debug interface of the game world.
 */
namespace WorldDebugUI {

/**
 * @brief Draws a table containing all entities and their primary components for a given registry.
 * 
 * @param label The header/title for the registry section.
 * @param reg The entt::registry to inspect.
 */
inline void DrawRegistry(const char* label, entt::registry& reg) {
    if (!ImGui::CollapsingHeader(label)) return;

    if (ImGui::BeginTable(label, 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("netId");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Rotation");
        ImGui::TableSetupColumn("Scale");
        ImGui::TableSetupColumn("Info");
        ImGui::TableHeadersRow();

        reg.view<entt::entity>().each([&](auto entity) {
            ImGui::PushID((int)entity);
            ImGui::TableNextRow();

            auto* n = reg.try_get<NetworkedComponent>(entity);
            auto* t = reg.try_get<TransformComponent>(entity);
            auto* p = reg.try_get<PlayerComponent>(entity);
            auto* m = reg.try_get<ModelComponent>(entity);
            auto* b = reg.try_get<BuildingComponent>(entity);

            // netId
            ImGui::TableSetColumnIndex(0);
            if (n) ImGui::Text("%u", n->netId); else ImGui::Text("-");

            // Type
            ImGui::TableSetColumnIndex(1);
            if (p) ImGui::Text("Player (%u)", p->playerId);
            else if (b) ImGui::Text("Building");
            else if (m) ImGui::Text("Asset");
            else ImGui::Text("Entity");

            // Transform manipulation
            if (t) {
                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##pos", &t->position.x, 0.1f);

                ImGui::TableSetColumnIndex(3);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##rot", &t->rotation.x, 0.1f);

                ImGui::TableSetColumnIndex(4);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##scale", &t->scale.x, 0.1f);
            } else {
                ImGui::TableSetColumnIndex(2); ImGui::Text("-");
                ImGui::TableSetColumnIndex(3); ImGui::Text("-");
                ImGui::TableSetColumnIndex(4); ImGui::Text("-");
            }

            // Info
            ImGui::TableSetColumnIndex(5);
            if (b) {
                ImGui::Text("T%u HP%.0f", b->currentTier, b->hp);
                if (b->isUpgrading) {
                    ImGui::SameLine();
                    ImGui::Text("(Up:%.1fs)", b->upgradeTimer);
                }
            }
            else if (m) ImGui::Text("%.20s...", m->modelPath.c_str());
            else ImGui::Text("-");

            ImGui::PopID();
        });

        ImGui::EndTable();
    }
}

/**
 * @brief Renders the main World Debug window.
 * 
 * Displays scene info, update statistics, and both server/client registries.
 * @param world Reference to the World instance to debug.
 */
inline void Draw(World& world) {
    ImGui::Begin("World Debug");

    ImGui::Text("Scene:       %s", world.GetCurrentSceneName());
    ImGui::Text("Accumulator: %.1f ms", world.GetAccumulator() * 1000.f);

    ImGui::Separator();
    // In a server-authoritative model, the host should only manipulate the Server Registry.
    // The Client Registry is just a reflected view.
    DrawRegistry("Server Registry (Authoritative)", world.GetServerRegistry());

    // Only show client registry to non-hosts, or as read-only for hosts to verify sync.
   
    ImGui::Separator();
    DrawRegistry("Client Registry (Visual/Interpolated)", world.GetClientRegistry());

    ImGui::End();
}

} // namespace WorldDebugUI

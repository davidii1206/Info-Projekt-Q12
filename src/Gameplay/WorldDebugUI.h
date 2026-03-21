#pragma once
#include <imgui.h>
#include <entt/entt.hpp>
#include "World.h"
#include "Components.h"

namespace WorldDebugUI {

inline void DrawRegistry(const char* label, entt::registry& reg) {
    if (!ImGui::CollapsingHeader(label)) return;

    auto view = reg.view<NetworkedComponent, TransformComponent, MovementComponent, PlayerComponent>();

    if (ImGui::BeginTable(label, 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("netId");
        ImGui::TableSetupColumn("playerId");
        ImGui::TableSetupColumn("local");
        ImGui::TableSetupColumn("position (x y z)");
        ImGui::TableSetupColumn("velocity (x y z)");
        ImGui::TableHeadersRow();

        for (auto entity : view) {
            auto& n  = view.get<NetworkedComponent>(entity);
            auto& t  = view.get<TransformComponent>(entity);
            auto& mv = view.get<MovementComponent>(entity);
            auto& p  = view.get<PlayerComponent>(entity);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%u",  n.netId);
            ImGui::TableSetColumnIndex(1); ImGui::Text("%u",  p.playerId);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%s",  p.isLocal ? "yes" : "no");
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f  %.2f  %.2f",
                                                        t.position.x, t.position.y, t.position.z);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f  %.2f  %.2f",
                                                        mv.velocity.x, mv.velocity.y, mv.velocity.z);
        }

        ImGui::EndTable();
    }
}

inline void Draw(World& world) {
    ImGui::Begin("World Debug");

    ImGui::Text("Scene:       %s", world.GetCurrentSceneName());
    ImGui::Text("Accumulator: %.1f ms", world.GetAccumulator() * 1000.f);

    ImGui::Separator();
    DrawRegistry("Server Registry", world.GetServerRegistry());

    ImGui::Separator();
    DrawRegistry("Client Registry", world.GetClientRegistry());

    ImGui::End();
}

} // namespace WorldDebugUI

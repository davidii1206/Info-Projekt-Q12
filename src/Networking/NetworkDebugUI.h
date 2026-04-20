/**
 * @file NetworkDebugUI.h
 * @brief ImGui-based user interface for managing network connections.
 */

#pragma once
#include "NetworkManager.h"
#include <imgui.h>
#include <cstring>

/**
 * @namespace NetDebug
 * @brief Contains UI drawing logic for networking.
 */
namespace NetDebug {

/**
 * @brief Renders the "Network" debug window.
 * 
 * Provides inputs for Host IP and Port, and buttons to Host, Join, or Disconnect.
 * @param net Reference to the NetworkManager instance.
 */
inline void Draw(NetworkManager& net) {
    ImGui::Begin("Network");

    // State label
    const char* stateLabel = "Idle";
    if (net.IsHosting())        stateLabel = "Hosting";
    else if (net.IsConnected()) stateLabel = "Connected";
    ImGui::Text("State: %s", stateLabel);

    ImGui::Separator();

    if (net.GetState() == NetworkState::Idle) {
        static char hostIP[64] = "127.0.0.1";
        static int  port       = 25565;

        ImGui::InputText("Host IP", hostIP, sizeof(hostIP));
        ImGui::InputInt("Port", &port);
        if (port < 1)     port = 1;
        if (port > 65535) port = 65535;

        if (ImGui::Button("Host")) {
            net.StartHost(static_cast<uint16_t>(port));
        }
        ImGui::SameLine();
        if (ImGui::Button("Join")) {
            net.Connect(hostIP, static_cast<uint16_t>(port));
        }
    } else {
        if (ImGui::Button("Disconnect")) {
            net.Disconnect();
        }
    }

    ImGui::End();
}

} // namespace NetDebug

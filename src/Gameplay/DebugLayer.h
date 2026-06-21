/**
 * @file DebugLayer.h
 * @brief Overlay layer that draws FPS and network debug ImGui windows.
 */

#pragma once
#include "../Core/Layer.h"
#include "../Core/Timer.h"
#include "../Networking/NetworkManager.h"
#include "../Networking/NetworkDebugUI.h"
#include <imgui.h>

/**
 * @class DebugLayer
 * @brief Developer overlay showing frame stats and networking debug info.
 */
class DebugLayer : public Layer {
public:
    /**
     * @brief Constructs the debug overlay.
     * @param timer   Frame timer (for FPS).
     * @param network Network manager (for net debug UI).
     */
    DebugLayer(Timer* timer, NetworkManager* network)
        : Layer("DebugLayer"), m_Timer(timer), m_Network(network) {}

    /// @brief Draws the debugger and network ImGui windows.
    void OnImGuiRender(Renderer* renderer) override {
        ImGui::Begin("Bugmin Debugger");
        ImGui::Text("FPS: %.1f", m_Timer->GetFPS());
        // We can add more general stats here
        ImGui::End();

        NetDebug::Draw(*m_Network);
    }

private:
    Timer* m_Timer;            ///< Frame timer (not owned).
    NetworkManager* m_Network; ///< Network manager (not owned).
};

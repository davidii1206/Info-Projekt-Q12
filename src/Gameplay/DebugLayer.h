#pragma once
#include "../Core/Layer.h"
#include "../Core/Timer.h"
#include "../Networking/NetworkManager.h"
#include "../Networking/NetworkDebugUI.h"
#include <imgui.h>

class DebugLayer : public Layer {
public:
    DebugLayer(Timer* timer, NetworkManager* network) 
        : Layer("DebugLayer"), m_Timer(timer), m_Network(network) {}

    void OnImGuiRender(Renderer* renderer) override {
        ImGui::Begin("Bugmin Debugger");
        ImGui::Text("FPS: %.1f", m_Timer->GetFPS());
        // We can add more general stats here
        ImGui::End();

        NetDebug::Draw(*m_Network);
    }

private:
    Timer* m_Timer;
    NetworkManager* m_Network;
};

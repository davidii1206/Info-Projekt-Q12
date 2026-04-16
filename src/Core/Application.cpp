/**
 * @file Application.cpp
 * @brief Implementation of the Application class.
 */

#include "Application.h"
#include "../Graphics/Renderer.h"
#include "../Gameplay/World.h"
#include "../Gameplay/PostProcessor.h"
#include "../Gameplay/WorldDebugUI.h"
#include "../Networking/NetworkDebugUI.h"
#include "Input.h"
#include "AssetManager.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <spdlog/spdlog.h>

Application::Application() {
    m_Window.title = "Bugmin Engine";
    m_Window.width = 1280;
    m_Window.height = 720;
    m_Window.mode = WindowMode::Windowed;
    m_Window.vsync = true;

    if (!CreateWindow(m_Window)) {
        spdlog::critical("Failed to create window");
        exit(1);
    }

    m_Renderer = std::make_unique<Renderer>(&m_Window);
    AssetManager::Init(m_Renderer->GetDevice());

    /// Initialize physics before world, as world needs the pointer.
    m_Physics.Init();
    m_Physics.AddStaticFloor();
    m_Physics.GetSystem().OptimizeBroadPhase();
    spdlog::info("Physics initialized");

    m_World = std::make_unique<World>(&m_Physics);

    m_PostProcessor = std::make_unique<PostProcessor>(m_Renderer.get());
}

Application::~Application() {
    m_PostProcessor.reset();
    m_World.reset();
    m_Physics.Shutdown();
    AssetManager::Shutdown();
    m_Renderer.reset();
    DestroyWindow(&m_Window);
}

void Application::Run() {
    while (m_Running) {
        m_Timer.Update();
        Input::Update();
        ProcessEvents();

        m_Network.Update();

        const float dt = m_Timer.GetDeltaTime();

        /// 1. Simulate physics and retrieve snapshots.
        const auto& snapshots = m_Physics.Step(dt);

        /// 2. Apply snapshots to Entity-Component system.
        m_World->ApplySnapshots(snapshots);

        if (m_Renderer->BeginFrame()) {
            if (m_PostProcessor) m_PostProcessor->BeginFrame(m_Renderer.get());

            m_World->Update(dt, m_Network, m_Renderer.get());
            m_World->Render(m_Renderer.get(), m_Network);

            if (m_PostProcessor) m_PostProcessor->EndFrame(m_Renderer.get());

            ImGui::Begin("Bugmin Debugger");
            ImGui::Text("FPS: %.1f", m_Timer.GetFPS());
            ImGui::Text("Physics steps: %lu", m_Physics.GetStepCount());
            ImGui::Text("Bodies tracked: %zu", snapshots.size());
            if (ImGui::Button("Exit")) m_Running = false;
            ImGui::End();

            if (m_PostProcessor) m_PostProcessor->OnImGui();
            
            NetDebug::Draw(m_Network);
            WorldDebugUI::Draw(*m_World);

            m_Renderer->EndFrame();
        }
    }
}

void Application::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        Input::ProcessEvent(event);
        if (event.type == SDL_EVENT_QUIT) m_Running = false;
    }
}

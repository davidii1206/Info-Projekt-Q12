// =============================================================================
// src/Core/Application.cpp
// =============================================================================

#include "Application.h"
#include "../Graphics/Renderer.h"
#include "../Gameplay/World.h"
#include "Input.h"
#include "AssetManager.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <spdlog/spdlog.h>

Application::Application() {
    AssetManager::Init();
    m_Window.title  = "Bugmin Engine";
    m_Window.width  = 1280;
    m_Window.height = 720;
    m_Window.mode   = WindowMode::Windowed;
    m_Window.vsync  = true;

    if (!CreateWindow(m_Window)) {
        spdlog::critical("Failed to create window");
        exit(1);
    }

    m_Renderer = std::make_unique<Renderer>(&m_Window);

    // Physics zuerst starten — World braucht den Pointer
    m_Physics.Init();
    m_Physics.AddStaticFloor();
    m_Physics.GetSystem().OptimizeBroadPhase();
    spdlog::info("Physics initialized");

    // World bekommt &m_Physics — spawnt dort ihre Entities
    m_World = std::make_unique<World>(&m_Physics);
}

Application::~Application() {
    // World zuerst — räumt Bodies in PhysicsServer auf
    m_World.reset();

    // Dann Physics herunterfahren
    m_Physics.Shutdown();

    m_Renderer.reset();
    DestroyWindow(&m_Window);
    AssetManager::Shutdown();
}

void Application::Run() {
    while (m_Running) {
        m_Timer.Update();
        Input::Update();
        ProcessEvents();

        const float dt = m_Timer.GetDeltaTime();

        // 1. Physics simulieren → Snapshots holen
        const auto& snapshots = m_Physics.Step(dt);

        // 2. Snapshots in entt-Components schreiben
        m_World->ApplySnapshots(snapshots);

        // 3. Game-Logik (Respawn, KI, Score, …)
        m_World->Update(dt);

        // 4. Rendern
        m_Renderer->BeginFrame();

        ImGui::Begin("Bugmin Debugger");
        ImGui::Text("FPS: %.1f",           m_Timer.GetFPS());
        ImGui::Text("Physics steps: %lu", m_Physics.GetStepCount());
        ImGui::Text("Bodies tracked: %zu", snapshots.size());
        if (ImGui::Button("Exit")) m_Running = false;
        ImGui::End();

        m_Renderer->EndFrame();
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

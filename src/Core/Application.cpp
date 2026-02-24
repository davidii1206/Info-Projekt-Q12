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
    m_World = std::make_unique<World>();
}

Application::~Application() {
    m_World.reset();
    m_Renderer.reset();
    DestroyWindow(&m_Window);
    AssetManager::Shutdown();
}

void Application::Run() {
    while (m_Running) {
        m_Timer.Update();
        Input::Update();
        ProcessEvents();

        m_World->Update(m_Timer.GetDeltaTime());

        m_Renderer->BeginFrame();
        
        ImGui::Begin("Bugmin Debugger");
        ImGui::Text("FPS: %.1f", m_Timer.GetFPS());
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
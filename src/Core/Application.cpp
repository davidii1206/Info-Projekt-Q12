#include "Application.h"
#include "../Graphics/Renderer.h"
#include "../Gameplay/World.h"
#include "../Gameplay/TestScene.h"
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
    m_World = std::make_unique<World>();

    m_TestScene = std::make_unique<TestScene>(m_Renderer.get());
}

Application::~Application() {
    m_TestScene.reset();
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
        if (m_TestScene) m_TestScene->Update(m_Timer.GetDeltaTime());

        if (m_Renderer->BeginFrame()) {
            if (m_TestScene) m_TestScene->Render(m_Renderer.get());

            ImGui::Begin("Bugmin Debugger");
            ImGui::Text("FPS: %.1f", m_Timer.GetFPS());
            if (ImGui::Button("Exit")) m_Running = false;
            ImGui::End();

            if (m_TestScene) m_TestScene->OnImGui();

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

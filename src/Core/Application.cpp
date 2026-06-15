/**
 * @file Application.cpp
 * @brief Implementation of the Application class using a layer-based architecture.
 */

#include "Application.h"
#include "../Graphics/Renderer.h"
#include "../Gameplay/GameLayer.h"
#include "../Gameplay/DebugLayer.h"
#include "../Audio/SoundSystem.h"
#include "Input.h"
#include "AssetManager.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Events/WindowEvent.h"

// Windows headers (pulled in transitively via enet/winsock) define macros that
// clash with our own method names. Undefine them after all includes.
#ifdef CreateWindow
#  undef CreateWindow
#endif
#ifdef PlaySound
#  undef PlaySound
#endif
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <spdlog/spdlog.h>

Application::Application() {
    // Initialize SoundSystem
    if (!SoundSystem::Get().Init()) {
        spdlog::warn("SoundSystem init failed – continuing without audio");
    }

    m_Window.title = "Bugmin Engine";
    m_Window.width = 1280;
    m_Window.height = 720;
    m_Window.mode   = WindowMode::Windowed;
    m_Window.vsync  = true;

    if (!CreateWindow(m_Window)) {
        spdlog::critical("Failed to create window");
        exit(1);
    }

    m_Renderer = std::make_unique<Renderer>(&m_Window);
    AssetManager::Init(m_Renderer->GetDevice());

    m_Physics.Init();
    // NOTE: The flat AddStaticFloor() below acts as a global safety net for
    // scenes that do not register their own mesh collision (e.g. MainMenuScene).
    // GameScene::OnEnter() registers accurate collision for the generated
    // terrain mesh on the host (see TerrainMeshBuilder).
    m_Physics.AddStaticFloor();
    m_Physics.GetSystem().OptimizeBroadPhase();
    spdlog::info("Physics initialized");

    // Push Layers
    PushLayer(new GameLayer(&m_Physics, &m_Network, m_Renderer.get(), &m_Timer));
    PushOverlay(new DebugLayer(&m_Timer, &m_Network));
    
    m_Timer.Reset();
}

Application::~Application() {
    /**
     * @brief Explicitly clear layers while the Renderer (and GPU Device) is still alive.
     * This prevents crashes when shaders/textures are released during shutdown.
     */
    m_LayerStack.Clear();

    m_Physics.Shutdown();
    AssetManager::Shutdown();
    SoundSystem::Get().Shutdown();

    // Automatic destruction will handle m_Renderer and m_Window in reverse declaration order.
}

void Application::PushLayer(Layer* layer) {
    m_LayerStack.PushLayer(layer);
}

void Application::PushOverlay(Layer* overlay) {
    m_LayerStack.PushOverlay(overlay);
}

void Application::Run() {
    while (m_Running) {
        m_Timer.Update();
        Input::Update();
        ProcessEvents();

        m_Network.Update();

        const float dt = m_Timer.GetDeltaTime();

        // 1. Update Layers (handles physics step & world logic)
        for (Layer* layer : m_LayerStack)
            layer->OnUpdate(dt);

        // 2. Update Sound System
        SoundSystem::Get().Update(dt);

        // 3. Render
        if (m_Renderer->BeginFrame()) {
            
            // Render 3D Scene
            for (Layer* layer : m_LayerStack)
                layer->OnRender(m_Renderer.get());

            // Render UI
            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender(m_Renderer.get());

            m_Renderer->EndFrame();
        }
    }
}

void Application::ProcessEvents() {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        ImGui_ImplSDL3_ProcessEvent(&sdlEvent);
        Input::ProcessEvent(sdlEvent);

        switch (sdlEvent.type) {
            case SDL_EVENT_QUIT: {
                WindowCloseEvent e;
                OnEvent(e);
                break;
            }
            case SDL_EVENT_WINDOW_RESIZED: {
                WindowResizeEvent e(sdlEvent.window.data1, sdlEvent.window.data2);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                KeyPressedEvent e(sdlEvent.key.key, sdlEvent.key.repeat);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_KEY_UP: {
                KeyReleasedEvent e(sdlEvent.key.key);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: {
                MouseMovedEvent e(sdlEvent.motion.x, sdlEvent.motion.y);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                MouseButtonPressedEvent e(sdlEvent.button.button);
                OnEvent(e);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                MouseButtonReleasedEvent e(sdlEvent.button.button);
                OnEvent(e);
                break;
            }
            default:
                break;
        }
    }
}

void Application::OnEvent(Event& event) {
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<WindowCloseEvent> ([this](WindowCloseEvent&  e) { return OnWindowClose(e);  });
    dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return OnWindowResize(e); });

    for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it) {
        if (event.Handled) break;
        (*it)->OnEvent(event);
    }
}

bool Application::OnWindowClose(WindowCloseEvent& /*e*/) {
    m_Running = false;
    return true;
}

bool Application::OnWindowResize(WindowResizeEvent& e) {
    m_Window.width  = e.GetWidth();
    m_Window.height = e.GetHeight();
    return false;
}

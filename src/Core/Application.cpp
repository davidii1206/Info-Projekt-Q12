#include "Application.h"
#include "../Graphics/Renderer.h"
#include "../Gameplay/GameLayer.h"
#include "../Audio/SoundSystem.h"
#include "Input.h"
#include "AssetManager.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Events/WindowEvent.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <spdlog/spdlog.h>

Application::Application() {
    AssetManager::Init();
    // Neu: SoundSystem initialisieren
    if (!SoundSystem::Get().Init()) {
        spdlog::warn("SoundSystem init failed – continuing without audio");
    }

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

    // Push the default gameplay layer
    PushLayer(new GameLayer());
}

Application::~Application() {
    // LayerStack destructor calls OnDetach() on all layers automatically
    m_Renderer.reset();
    DestroyWindow(&m_Window);
    AssetManager::Shutdown();

    //soundsystem hernuterfahren
    SoundSystem::Get().Shutdown();
}

// ── Public API ───────────────────────────────────────────────────────────────

void Application::PushLayer(Layer* layer) {
    m_LayerStack.PushLayer(layer);
}

void Application::PushOverlay(Layer* overlay) {
    m_LayerStack.PushOverlay(overlay);
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void Application::Run() {
    while (m_Running) {
        m_Timer.Update();
        Input::Update();
        ProcessEvents();

        // Update all layers front → back
        for (Layer* layer : m_LayerStack)
            layer->OnUpdate(m_Timer.GetDeltaTime());

        m_Renderer->BeginFrame();

        // ImGui rendering for all layers front → back
        for (Layer* layer : m_LayerStack)
            layer->OnImGuiRender();

        // Built-in debug overlay
        ImGui::Begin("Bugmin Debugger");
        ImGui::Text("FPS: %.1f", m_Timer.GetFPS());
        if (ImGui::Button("Exit")) m_Running = false;
        ImGui::End();

        m_Renderer->EndFrame();
    }
}

// ── Event processing ─────────────────────────────────────────────────────────

void Application::ProcessEvents() {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        ImGui_ImplSDL3_ProcessEvent(&sdlEvent);
        Input::ProcessEvent(sdlEvent);

        // Translate SDL events → our Event types
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
    // Application-level handlers first
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<WindowCloseEvent> ([this](WindowCloseEvent&  e) { return OnWindowClose(e);  });
    dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) { return OnWindowResize(e); });

    // Propagate to layers back → front (overlays receive events first)
    for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it) {
        if (event.Handled) break;
        (*it)->OnEvent(event);
    }
}

bool Application::OnWindowClose(WindowCloseEvent& /*e*/) {
    m_Running = false;
    return true; // consumed
}

bool Application::OnWindowResize(WindowResizeEvent& e) {
    m_Window.width  = e.GetWidth();
    m_Window.height = e.GetHeight();
    return false; // let layers know too
}

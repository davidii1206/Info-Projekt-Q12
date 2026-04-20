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

// Win32 defines CreateWindow/DestroyWindow as macros (→ CreateWindowA/W).
// Undefine them here so our own engine functions with the same names are usable.
#ifdef CreateWindow
#  undef CreateWindow
#endif
#ifdef DestroyWindow
#  undef DestroyWindow
#endif

Application::Application() {
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
    AssetManager::Init(m_Renderer->GetDevice());

    // Initialize audio
    if (!SoundSystem::Get().Init()) {
        spdlog::warn("SoundSystem init failed – continuing without audio");
    }

    // Initialize physics before world, as world needs the pointer.
    m_Physics.Init();
    m_Physics.AddStaticFloor();
    m_Physics.GetSystem().OptimizeBroadPhase();
    spdlog::info("Physics initialized");

    m_World = std::make_unique<World>(&m_Physics);

    m_PostProcessor = std::make_unique<PostProcessor>(m_Renderer.get());

    // Push the default gameplay layer
    PushLayer(new GameLayer(&m_Physics));
}

Application::~Application() {
    // LayerStack destructor calls OnDetach() on all layers automatically
    m_PostProcessor.reset();
    m_World.reset();
    m_Physics.Shutdown();
    m_Renderer.reset();
    DestroyWindow(&m_Window);
    AssetManager::Shutdown();
    SoundSystem::Get().Shutdown();
}

// ── Public API ────────────────────────────────────────────────────────────────

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

        m_Network.Update();

        const float dt = m_Timer.GetDeltaTime();

        // 1. Simulate physics and retrieve snapshots.
        const auto& snapshots = m_Physics.Step(dt);

        // 2. Apply snapshots to Entity-Component system.
        m_World->ApplySnapshots(snapshots);

        // Update all layers front → back
        for (Layer* layer : m_LayerStack)
            layer->OnUpdate(dt);

        if (m_Renderer->BeginFrame()) {
            if (m_PostProcessor) m_PostProcessor->BeginFrame(m_Renderer.get());

            m_World->Update(dt, m_Network, m_Renderer.get());
            m_World->Render(m_Renderer.get(), m_Network);

            if (m_PostProcessor) m_PostProcessor->EndFrame(m_Renderer.get());

            // ImGui rendering for all layers front → back
            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender();

            // Built-in debug overlay
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

// ── Event processing ──────────────────────────────────────────────────────────

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
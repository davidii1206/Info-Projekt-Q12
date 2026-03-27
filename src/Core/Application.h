#pragma once
#include <memory>
#include <string>
#include "Window/Window.h"
#include "Core/Timer.h"
#include "Core/LayerStack.h"
#include "Core/Events/Event.h"
#include "Core/Events/WindowEvent.h"

class Renderer;

class Application {
public:
    Application();
    ~Application();

    void Run();

    // Push a regular layer (below overlays)
    void PushLayer(Layer* layer);
    // Push an overlay (always on top, receives events first)
    void PushOverlay(Layer* overlay);

private:
    void ProcessEvents();
    void OnEvent(Event& event);

    // Built-in event handlers
    bool OnWindowResize(WindowResizeEvent& e);
    bool OnWindowClose(WindowCloseEvent& e);

    Window                  m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    Timer                   m_Timer;
    LayerStack              m_LayerStack;
    bool                    m_Running = true;
};

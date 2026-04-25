/**
 * @file Application.h
 * @brief Main application class for the Bugmin engine.
 */

#pragma once
#include <memory>
#include <string>
#include "Window/Window.h"
#include "Core/Timer.h"
#include "Networking/NetworkManager.h"
#include "Core/PhysicsServer.h"
#include "Core/LayerStack.h"
#include "Core/Events/Event.h"
#include "Core/Events/WindowEvent.h"

class Renderer;

/**
 * @class Application
 * @brief The main entry point and controller for the Bugmin engine.
 */
class Application {
public:
    Application();
    ~Application();

    void Run();

    void PushLayer(Layer* layer);
    void PushOverlay(Layer* overlay);

private:
    void ProcessEvents();
    void OnEvent(Event& event);

    bool OnWindowResize(WindowResizeEvent& e);
    bool OnWindowClose(WindowCloseEvent& e);

    Window m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    PhysicsServer             m_Physics;
    NetworkManager            m_Network;
    Timer                     m_Timer;
    LayerStack                m_LayerStack;
    bool                      m_Running = true;
};

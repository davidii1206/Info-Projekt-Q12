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
    Application();  ///< Creates the window, renderer and core services.
    ~Application(); ///< Tears down all services.

    /// @brief Runs the main loop until the window closes.
    void Run();

    /// @brief Pushes a regular layer onto the layer stack.
    void PushLayer(Layer* layer);
    /// @brief Pushes an overlay onto the layer stack.
    void PushOverlay(Layer* overlay);

private:
    /// @brief Pumps SDL events and forwards them to OnEvent().
    void ProcessEvents();
    /// @brief Dispatches an event to handlers and the layer stack.
    void OnEvent(Event& event);

    /// @brief Handles window resize (recreates swapchain-dependent state).
    bool OnWindowResize(WindowResizeEvent& e);
    /// @brief Handles window close (stops the main loop).
    bool OnWindowClose(WindowCloseEvent& e);

    Window m_Window;                        ///< The application window.
    std::unique_ptr<Renderer> m_Renderer;   ///< The renderer (owned).
    PhysicsServer             m_Physics;     ///< Shared physics server.
    NetworkManager            m_Network;     ///< Shared network manager.
    Timer                     m_Timer;       ///< Frame timer.
    LayerStack                m_LayerStack;  ///< Stack of active layers.
    bool                      m_Running = true; ///< Main-loop run flag.
};

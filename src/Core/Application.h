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
class World;
class PostProcessor;

/**
 * @class Application
 * @brief The main entry point and controller for the Bugmin engine.
 * 
 * This class manages the primary game loop, initializes core systems 
 * (Renderer, World, Input), and handles top-level system events.
 */
class Application {
public:
    /**
     * @brief Initializes the engine, creating the window and core systems.
     */
    Application();

    /**
     * @brief Cleans up all engine systems and resources.
     */
    ~Application();

    /**
     * @brief Starts and maintains the main execution loop.
     * 
     * This function runs until the application is requested to close.
     */
    void Run();

    // Push a regular layer (below overlays)
    void PushLayer(Layer* layer);
    // Push an overlay (always on top, receives events first)
    void PushOverlay(Layer* overlay);

private:
    /**
     * @brief Processes SDL system events.
     * 
     * Handles window events, keyboard/mouse input, and quit requests.
     */
    void ProcessEvents();
    void OnEvent(Event& event);

    // Built-in event handlers
    bool OnWindowResize(WindowResizeEvent& e);
    bool OnWindowClose(WindowCloseEvent& e);

    Window m_Window; ///< Main application window.
    std::unique_ptr<Renderer> m_Renderer; ///< Core renderer system.
    PhysicsServer             m_Physics;   ///< Jolt physics server.
    std::unique_ptr<World>    m_World; ///< Game world containing entities and systems.
    std::unique_ptr<PostProcessor> m_PostProcessor; ///< Post-processing system.
    Timer m_Timer; ///< Frame timer for delta time calculations.
    NetworkManager m_Network; ///< Networking system.
    LayerStack              m_LayerStack;
    bool m_Running = true; ///< Main loop execution flag.
};

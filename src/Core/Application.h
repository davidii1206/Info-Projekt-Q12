/**
 * @file Application.h
 * @brief Main application class for the Bugmin engine.
 */

#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Window/Window.h"
#include "Core/Timer.h"
#include "Core/LayerStack.h"
#include "Core/Events/Event.h"
#include "Core/Events/WindowEvent.h"
#include "Networking/NetworkManager.h"
#include "Core/PhysicsServer.h"

class Renderer;
class World;
class PostProcessor;

/**
 * @class Application
 * @brief The main entry point and controller for the Bugmin engine.
 *
 * This class manages the primary game loop, initializes core systems
 * (Renderer, World, Physics, Audio, Input, Networking), and handles
 * top-level system events via the typed event dispatch system.
 */
class Application {
public:
    /**
     * @brief Initializes the engine, creating the window and all core systems.
     */
    Application();

    /**
     * @brief Cleans up all engine systems and resources.
     */
    ~Application();

    /**
     * @brief Starts and maintains the main execution loop.
     *
     * Runs until the application is requested to close.
     */
    void Run();

    /** @brief Pushes a regular layer onto the layer stack (below overlays). */
    void PushLayer(Layer* layer);

    /** @brief Pushes an overlay onto the layer stack (always on top, receives events first). */
    void PushOverlay(Layer* overlay);

private:
    /**
     * @brief Polls and translates SDL events into typed engine events.
     */
    void ProcessEvents();

    /**
     * @brief Dispatches a typed engine event to application handlers and the layer stack.
     */
    void OnEvent(Event& event);

    // Built-in application-level event handlers
    bool OnWindowClose(WindowCloseEvent& e);
    bool OnWindowResize(WindowResizeEvent& e);

    Window                         m_Window;         ///< Main application window.
    std::unique_ptr<Renderer>      m_Renderer;       ///< Core renderer system.
    PhysicsServer                  m_Physics;        ///< Jolt physics server.
    std::unique_ptr<World>         m_World;          ///< Game world containing entities and systems.
    std::unique_ptr<PostProcessor> m_PostProcessor;  ///< Post-processing system.
    NetworkManager                 m_Network;        ///< Networking system.
    LayerStack                     m_LayerStack;     ///< Ordered stack of game/UI layers.
    Timer                          m_Timer;          ///< Frame timer for delta time calculations.
    bool                           m_Running = true; ///< Main loop execution flag.
};
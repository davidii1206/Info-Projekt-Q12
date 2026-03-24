#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Window/Window.h"
#include "Core/Timer.h"
#include "Networking/NetworkManager.h"
#include "Core/PhysicsServer.h"

class Renderer;
class World;
class TestScene;

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

private:
    /**
     * @brief Processes SDL system events.
     * 
     * Handles window events, keyboard/mouse input, and quit requests.
     */
    void ProcessEvents();

    Window m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    PhysicsServer             m_Physics;   // vor m_World — World braucht den Pointer
    std::unique_ptr<World>    m_World;
    std::unique_ptr<TestScene> m_TestScene;
    Timer m_Timer;
    NetworkManager m_Network;
    bool m_Running = true;
};

#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Window/Window.h"
#include "Core/Timer.h"
#include "Networking/NetworkManager.h"

class Renderer;
class World;

class Application {
public:
    Application();
    ~Application();

    void Run();

private:
    void ProcessEvents();

    Window m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<World> m_World;
    Timer m_Timer;
    NetworkManager m_Network;
    bool m_Running = true;
};
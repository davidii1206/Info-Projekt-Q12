#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Window/Window.h"
#include "Core/Timer.h"
#include "Core/PhysicsServer.h"

class Renderer;
class World;

class Application {
public:
    Application();
    ~Application();

    void Run();

private:
    void ProcessEvents();

    Window                    m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    PhysicsServer             m_Physics;   // vor m_World — World braucht den Pointer
    std::unique_ptr<World>    m_World;
    Timer                     m_Timer;
    bool                      m_Running = true;
};

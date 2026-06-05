#pragma once
#include "../Core/Layer.h"
#include "../Audio/SoundSystem.h"
#include "../Audio/SoundEvents.h"
#include "World.h"
#include "PostProcessor.h"
#include "../Networking/NetworkManager.h"
#include <memory>

class Timer;

class GameLayer : public Layer {
public:
    GameLayer(PhysicsServer* physics, NetworkManager* network, Renderer* renderer, Timer* timer);
    virtual ~GameLayer() = default;

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender(Renderer* renderer) override;
    void OnImGuiRender(Renderer* renderer) override;
    void OnEvent(Event& event) override;

private:
    bool OnEntityDamaged(EntityDamagedEvent& e);
    bool OnEntityDied(EntityDiedEvent& e);
    bool OnBiomeChanged(BiomeChangedEvent& e);

    PhysicsServer* m_Physics;
    NetworkManager* m_Network;
    Renderer* m_Renderer;
    Timer* m_Timer;
    std::unique_ptr<World> m_World;
    std::unique_ptr<PostProcessor> m_PostProcessor;
};

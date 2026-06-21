/**
 * @file GameLayer.h
 * @brief The main gameplay layer: owns the World, post-processor and wires
 *        gameplay events into the SoundSystem.
 */

#pragma once
#include "../Core/Layer.h"
#include "../Audio/SoundSystem.h"
#include "../Audio/SoundEvents.h"
#include "World.h"
#include "PostProcessor.h"
#include "../Networking/NetworkManager.h"
#include <memory>

class Timer;

/**
 * @class GameLayer
 * @brief Drives the game world each frame and renders it.
 *
 * Holds the World (registries + scenes), the PostProcessor, and references to
 * the shared physics/network/renderer/timer services. Translates gameplay
 * events (damage, death, biome change) into audio.
 */
class GameLayer : public Layer {
public:
    /**
     * @brief Constructs the game layer.
     * @param physics  Shared physics server.
     * @param network  Shared network manager.
     * @param renderer Active renderer.
     * @param timer    Frame timer.
     */
    GameLayer(PhysicsServer* physics, NetworkManager* network, Renderer* renderer, Timer* timer);
    virtual ~GameLayer() = default; ///< Default destructor.

    void OnAttach() override;                    ///< Creates the World/post-processor.
    void OnDetach() override;                    ///< Tears down owned resources.
    void OnUpdate(float dt) override;            ///< Advances the world (logic + fixed tick).
    void OnRender(Renderer* renderer) override;  ///< Renders the world.
    void OnImGuiRender(Renderer* renderer) override; ///< Renders debug UI.
    void OnEvent(Event& event) override;         ///< Dispatches incoming events.

private:
    bool OnEntityDamaged(EntityDamagedEvent& e); ///< Plays a hit sound.
    bool OnEntityDied(EntityDiedEvent& e);       ///< Plays a death sound.
    bool OnBiomeChanged(BiomeChangedEvent& e);   ///< Crossfades biome ambient.

    PhysicsServer* m_Physics;   ///< Shared physics server (not owned).
    NetworkManager* m_Network;  ///< Shared network manager (not owned).
    Renderer* m_Renderer;       ///< Active renderer (not owned).
    Timer* m_Timer;             ///< Frame timer (not owned).
    std::unique_ptr<World> m_World;                 ///< The game world (owned).
    std::unique_ptr<PostProcessor> m_PostProcessor; ///< Post-processing stack (owned).
};

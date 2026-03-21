#include "World.h"
#include "MainMenuScene.h"
#include "../Networking/NetworkManager.h"
#include <spdlog/spdlog.h>

World::World() {
    m_SceneManager.RequestTransition(new MainMenuScene());
    spdlog::info("World: initialized, starting in MainMenuScene");
}

World::~World() {
}

void World::Update(float dt, NetworkManager& net) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager};

    // Apply any pending scene transition at the top of the frame,
    // before any updates run, to keep OnEnter/OnExit clean.
    m_SceneManager.Flush(ctx);

    m_Accumulator += dt;
    while (m_Accumulator >= FIXED_DT) {
        FixedUpdate(FIXED_DT, net);
        m_Accumulator -= FIXED_DT;
    }

    m_SceneManager.FrameUpdate(ctx, dt);
}

void World::Update(float dt) {
    // Legacy update if needed, but we should prefer the networking one
}

void World::FixedUpdate(float dt, NetworkManager& net) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager};
    m_SceneManager.FixedUpdate(ctx, dt);
}

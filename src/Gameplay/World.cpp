#include "World.h"
#include "MainMenuScene.h"
#include "../Networking/NetworkManager.h"
#include <spdlog/spdlog.h>

World::World(PhysicsServer* physics)
    : m_Physics(physics)
{
    m_SceneManager.RequestTransition(new MainMenuScene());
    spdlog::info("World: initialized, starting in MainMenuScene");
}

World::~World() {
    // Bodies in PhysicsServer aufräumen (für ServerRegistry)
    auto view = m_ServerRegistry.view<PhysicsBodyComponent>();
    for (auto entity : view) {
        auto& body = view.get<PhysicsBodyComponent>(entity);
        if (m_Physics && body.handle.IsValid())
            m_Physics->RemoveBody(body.handle);
    }
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
    // Legacy update if needed
}

void World::Render(Renderer* renderer, NetworkManager& net) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager};
    m_SceneManager.Render(ctx, renderer);
}

void World::ApplySnapshots(const std::vector<TransformSnapshot>& snapshots) {
    for (const auto& snap : snapshots) {
        auto it = m_IDToEntity.find(snap.entityID);
        if (it == m_IDToEntity.end()) continue;

        auto& tf = m_ServerRegistry.get<TransformComponent>(it->second);
        tf.position = glm::vec3(snap.position.GetX(), snap.position.GetY(), snap.position.GetZ());
        
        JPH::Vec3 euler = snap.rotation.GetEulerAngles();
        tf.rotation = glm::vec3(euler.GetX(), euler.GetY(), euler.GetZ());
    }
}

void World::RegisterPhysicsEntity(uint32_t id, entt::entity entity) {
    m_IDToEntity[id] = entity;
}

void World::UnregisterPhysicsEntity(uint32_t id) {
    m_IDToEntity.erase(id);
}

void World::FixedUpdate(float dt, NetworkManager& net) {
    SceneContext ctx{m_ServerRegistry, m_ClientRegistry, net, m_SceneManager};
    m_SceneManager.FixedUpdate(ctx, dt);
}

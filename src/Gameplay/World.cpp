// =============================================================================
// src/Gameplay/World.cpp
// =============================================================================

#include "World.h"
#include <spdlog/spdlog.h>

World::World(PhysicsServer* physics)
    : m_Physics(physics)
{
    SpawnInitialEntities();
    spdlog::info("World initialized with {} entities", m_IDToEntity.size());
}

World::~World() {
    // Bodies in PhysicsServer aufräumen
    auto view = m_Registry.view<PhysicsBodyComponent>();
    for (auto entity : view) {
        auto& body = view.get<PhysicsBodyComponent>(entity);
        if (m_Physics && body.handle.IsValid())
            m_Physics->RemoveBody(body.handle);
    }
}

// ---------------------------------------------------------------------------
// SpawnInitialEntities — Beispiel-Entities anlegen
// ---------------------------------------------------------------------------
void World::SpawnInitialEntities() {
    for (int i = 0; i < 5; ++i) {
        uint32_t eid = m_NextEntityID++;

        // 1. entt-Entity erstellen
        entt::entity e = m_Registry.create();

        // 2. Jolt-Body erstellen und an entt-Entity hängen
        PhysicsBodyHandle handle = m_Physics->AddDynamicBox(
            eid,
            JPH::RVec3(static_cast<float>(i) * 2.f, 10.f + i, 0.f),
            JPH::Vec3(0.4f, 0.9f, 0.4f),
            /*restitution=*/   0.1f,
            /*linearDamping=*/ 0.9f
        );

        m_Registry.emplace<EntityIDComponent>   (e, eid);
        m_Registry.emplace<PhysicsBodyComponent>(e, handle);
        m_Registry.emplace<TransformComponent>  (e);  // startet bei (0,0,0)

        // Für schnellen Lookup beim ApplySnapshots
        m_IDToEntity[eid] = e;
    }
}

// ---------------------------------------------------------------------------
// Update — Game-Logik, läuft jeden Frame
// ---------------------------------------------------------------------------
void World::Update(float dt) {
    (void)dt;

    // Beispiel: Respawn-Check anhand der aktuellen TransformComponent
    auto view = m_Registry.view<TransformComponent, EntityIDComponent>();
    for (auto entity : view) {
        auto& tf  = view.get<TransformComponent>(entity);
        auto& eid = view.get<EntityIDComponent>(entity);

        if (tf.position.GetY() < -20.f) {
            spdlog::warn("Entity {} unter Y=-20, respawning", eid.id);

            auto* body = m_Registry.try_get<PhysicsBodyComponent>(entity);
            if (body && body->handle.IsValid()) {
                m_Physics->Teleport(
                    body->handle,
                    JPH::RVec3(0.f, 5.f, 0.f),
                    JPH::Quat::sIdentity()
                );
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ApplySnapshots — Physics-Ergebnisse in entt-Components schreiben
// Wird von Application nach m_Physics.Step() aufgerufen.
// ---------------------------------------------------------------------------
void World::ApplySnapshots(const std::vector<TransformSnapshot>& snapshots) {
    for (const auto& snap : snapshots) {
        auto it = m_IDToEntity.find(snap.entityID);
        if (it == m_IDToEntity.end()) continue;

        auto& tf = m_Registry.get<TransformComponent>(it->second);
        tf.position = snap.position;
        tf.rotation = snap.rotation;
    }
}

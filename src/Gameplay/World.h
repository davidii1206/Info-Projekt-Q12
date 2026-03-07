#pragma once

// =============================================================================
// src/Gameplay/World.h
// Verwaltet alle Spiel-Entities mit EnTT.
// Bekommt einen PhysicsServer-Pointer um Bodies zu spawnen/steuern.
// =============================================================================

#include <entt/entt.hpp>
#include "../Core/PhysicsServer.h"

// ---------------------------------------------------------------------------
// Components — an entt-Entity hängen
// ---------------------------------------------------------------------------

// Verlinkt eine entt-Entity mit einem Jolt-Body
struct PhysicsBodyComponent {
    PhysicsBodyHandle handle;
};

// Aktuelle Weltposition (wird jeden Frame von Physics überschrieben)
struct TransformComponent {
    JPH::RVec3 position = JPH::RVec3::sZero();
    JPH::Quat  rotation = JPH::Quat::sIdentity();
};

// Optionales Tag damit World weiß welche entityID zu welcher entt-Entity gehört
struct EntityIDComponent {
    uint32_t id = 0;
};

// ---------------------------------------------------------------------------
// World
// ---------------------------------------------------------------------------
class World {
public:
    // PhysicsServer wird von Application übergeben — World besitzt ihn NICHT
    explicit World(PhysicsServer* physics);
    ~World();

    void Update(float dt);

    // Snapshots von PhysicsServer in entt-Components schreiben
    void ApplySnapshots(const std::vector<TransformSnapshot>& snapshots);

    entt::registry& GetRegistry() { return m_Registry; }

private:
    void SpawnInitialEntities();

    PhysicsServer*  m_Physics = nullptr;   // nicht-owning Pointer
    entt::registry  m_Registry;
    uint32_t        m_NextEntityID = 1;

    // Schnelle Suche: physikalische entityID → entt entity
    std::unordered_map<uint32_t, entt::entity> m_IDToEntity;
};

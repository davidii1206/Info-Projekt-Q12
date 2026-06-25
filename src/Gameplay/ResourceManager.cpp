/**
 * @file ResourceManager.cpp
 * @brief Implementation of the ResourceManager.
 */

#include "ResourceManager.h"
#include "Components.h"
#include <spdlog/spdlog.h>
#include <random>

// ---------------------------------------------------------------------------
// Init – define all permanent spawn points for the map
// ---------------------------------------------------------------------------

void ResourceManager::Init()
{
    // Permanent resource nodes – positions are placeholders and should be
    // updated once the final map geometry is set in Blender.
    m_SpawnPoints = {
        // Pilze (Mushrooms) – shaded corners
        { glm::vec3{  5.f, 0.f,  5.f }, ResourceType::Pilze,    3, 30.f },
        { glm::vec3{ -5.f, 0.f,  5.f }, ResourceType::Pilze,    3, 30.f },
        { glm::vec3{  5.f, 0.f, -5.f }, ResourceType::Pilze,    3, 30.f },
        { glm::vec3{ -5.f, 0.f, -5.f }, ResourceType::Pilze,    3, 30.f },

        // Beeren (Berries) – open area
        { glm::vec3{  8.f, 0.f,  0.f }, ResourceType::Beeren,   5, 25.f },
        { glm::vec3{ -8.f, 0.f,  0.f }, ResourceType::Beeren,   5, 25.f },
        { glm::vec3{  0.f, 0.f,  8.f }, ResourceType::Beeren,   5, 25.f },
        { glm::vec3{  0.f, 0.f, -8.f }, ResourceType::Beeren,   5, 25.f },

        // Nektar (Nectar) – flower patches
        { glm::vec3{  3.f, 0.f,  7.f }, ResourceType::Nektar,   2, 40.f },
        { glm::vec3{ -3.f, 0.f, -7.f }, ResourceType::Nektar,   2, 40.f },

        // Samen (Seeds) – grass patches
        { glm::vec3{  0.f, 0.f,  4.f }, ResourceType::Samen,    4, 20.f },
        { glm::vec3{  4.f, 0.f,  0.f }, ResourceType::Samen,    4, 20.f },
        { glm::vec3{ -4.f, 0.f,  0.f }, ResourceType::Samen,    4, 20.f },
        { glm::vec3{  0.f, 0.f, -4.f }, ResourceType::Samen,    4, 20.f },

        // Insekten (Insects) – sparse, high value
        { glm::vec3{  6.f, 0.f,  6.f }, ResourceType::Insekten, 1, 60.f },
        { glm::vec3{ -6.f, 0.f, -6.f }, ResourceType::Insekten, 1, 60.f },

        // Fleisch is NOT permanent – it is dropped by bosses/enemies only.
        // No permanent spawn points for Fleisch.
    };

    spdlog::info("[ResourceManager] Initialized with {} permanent spawn points.",
                 m_SpawnPoints.size());
}

// ---------------------------------------------------------------------------
// GenerateForWorld – procedurally distribute spawn points across the map
// ---------------------------------------------------------------------------

void ResourceManager::GenerateForWorld(float worldExtent, uint32_t seed)
{
    m_SpawnPoints.clear();

    // Per-type counts and respawn timings. Holz is the heaviest because it
    // drives universal base upgrades; cooldown is short so workers can keep
    // chopping. Insekten is rare/slow on purpose (high-value carnivore food).
    struct TypeSpec {
        ResourceType type;
        int          count;
        int          amount;
        float        respawn;
    };
    const TypeSpec specs[] = {
        { ResourceType::Holz,     90, 5, 10.f },
        { ResourceType::Pilze,    35, 3, 25.f },
        { ResourceType::Beeren,   35, 4, 25.f },
        { ResourceType::Nektar,   25, 3, 30.f },
        { ResourceType::Samen,    35, 4, 20.f },
        { ResourceType::Insekten, 20, 2, 45.f },
    };

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-worldExtent, worldExtent);

    for (const auto& s : specs) {
        for (int i = 0; i < s.count; ++i) {
            // Y is finalised when the node spawns (snapped to terrain there).
            m_SpawnPoints.push_back({
                glm::vec3{ dist(rng), 0.f, dist(rng) },
                s.type, s.amount, s.respawn
            });
        }
    }

    spdlog::info("[ResourceManager] Generated {} spawn points across {}x{} map.",
                 m_SpawnPoints.size(), (int)(worldExtent * 2), (int)(worldExtent * 2));
}

// ---------------------------------------------------------------------------
// SpawnPermanentResources
// ---------------------------------------------------------------------------

void ResourceManager::SpawnPermanentResources(entt::registry& registry)
{
    for (const auto& sp : m_SpawnPoints) {
        auto entity = registry.create();
        registry.emplace<TransformComponent>(entity, sp.position);
        registry.emplace<ResourceComponent>(entity,
            sp.type, sp.amount, /*permanent=*/true, sp.respawnTime);
    }

    spdlog::info("[ResourceManager] Spawned {} permanent resource nodes.",
                 m_SpawnPoints.size());
}

// ---------------------------------------------------------------------------
// Update – tick respawn timers
// ---------------------------------------------------------------------------

void ResourceManager::Update(entt::registry& registry, float dt)
{
    // Collect entities to destroy after the loop to avoid iterator invalidation.
    std::vector<entt::entity> toDestroy;

    auto view = registry.view<ResourceComponent>();
    for (auto entity : view) {
        auto& res = view.get<ResourceComponent>(entity);

        if (res.permanent) {
            // Permanent nodes: tick respawn timer when depleted.
            if (!res.depleted) continue;
            res.respawnTimer -= dt;
            if (res.respawnTimer <= 0.f)
                RespawnNode(registry, entity);
        } else {
            // Non-permanent drops: tick lifetime and destroy when expired.
            res.respawnTimer -= dt;
            if (res.respawnTimer <= 0.f) {
                spdlog::debug("[ResourceManager] Drop expired ({}), destroying entity.",
                              ResourceTypeName(res.type));
                toDestroy.push_back(entity);
            }
        }
    }

    for (auto e : toDestroy)
        if (registry.valid(e))
            registry.destroy(e);
}

// ---------------------------------------------------------------------------
// Collect
// ---------------------------------------------------------------------------

int ResourceManager::Collect(entt::registry& registry,
                              entt::entity    node,
                              ResourceType&   outType)
{
    if (!registry.valid(node))
        return 0;

    auto* res = registry.try_get<ResourceComponent>(node);
    if (!res || res->depleted)
        return 0;

    outType = res->type;
    int collected = 1; // collect one unit per interaction
    res->amount -= collected;

    if (res->amount <= 0) {
        res->depleted = true;
        res->respawnTimer = res->respawnTime;

        if (!res->permanent) {
            // Non-permanent nodes (drops) are destroyed immediately
            registry.destroy(node);
        } else {
            spdlog::debug("[ResourceManager] Node depleted ({}) – respawn in {:.0f}s",
                          ResourceTypeName(res->type), res->respawnTime);
        }
    }

    return collected;
}

// ---------------------------------------------------------------------------
// RespawnNode
// ---------------------------------------------------------------------------

void ResourceManager::RespawnNode(entt::registry& registry, entt::entity entity)
{
    auto* res = registry.try_get<ResourceComponent>(entity);
    if (!res) return;

    // Find the matching spawn point to restore original amount
    const auto* tf = registry.try_get<TransformComponent>(entity);
    int originalAmount = 1;
    if (tf) {
        for (const auto& sp : m_SpawnPoints) {
            if (glm::distance(sp.position, tf->position) < 0.1f &&
                sp.type == res->type) {
                originalAmount = sp.amount;
                break;
            }
        }
    }

    res->amount       = originalAmount;
    res->depleted     = false;
    res->respawnTimer = 0.f;

    spdlog::debug("[ResourceManager] Node respawned ({})", ResourceTypeName(res->type));
}
// ---------------------------------------------------------------------------
// SpawnMeatDrop
// ---------------------------------------------------------------------------

void ResourceManager::SpawnMeatDrop(entt::registry& registry,
                                    glm::vec3       position,
                                    int             amount,
                                    float           dropLifetime)
{
    auto entity = registry.create();
    registry.emplace<TransformComponent>(entity, position);

    // permanent = false  →  Update() will count down respawnTimer as the
    // drop lifetime and destroy the entity once it reaches 0.
    ResourceComponent rc(ResourceType::Fleisch, amount, /*permanent=*/false, dropLifetime);
    rc.respawnTimer = dropLifetime; // countdown starts immediately
    registry.emplace<ResourceComponent>(entity, rc);

    spdlog::info("[ResourceManager] Spawned Fleisch drop at ({:.1f},{:.1f},{:.1f})"
                 " – disappears in {:.0f}s",
                 position.x, position.y, position.z, dropLifetime);
}

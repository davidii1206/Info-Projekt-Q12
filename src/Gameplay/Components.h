#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include "../Core/PhysicsServer.h"

struct TransformComponent {
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 scale{1.f};

    TransformComponent() = default;
    explicit TransformComponent(glm::vec3 pos) : position(pos) {}
};

struct MovementComponent {
    glm::vec3 velocity{0.f};
    float     speed = 5.f;
    glm::vec2 inputDir{0.f}; // XZ-plane direction set from input or server
};

struct PlayerComponent {
    uint32_t playerId = 0;
    bool     isLocal  = false;
};

// Links an entity to a stable network identity.
// Client entities match server entities via this id.
struct NetworkedComponent {
    uint32_t netId = 0;
};

struct ModelComponent {
    std::string modelPath;
};

// ---------------------------------------------------------------------------
// Physics Components
// ---------------------------------------------------------------------------

// Verlinkt eine entt-Entity mit einem Jolt-Body
struct PhysicsBodyComponent {
    PhysicsBodyHandle handle;
};

// Optionales Tag damit World weiß welche entityID zu welcher entt-Entity gehört
struct EntityIDComponent {
    uint32_t id = 0;
};

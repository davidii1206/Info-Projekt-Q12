#pragma once
#include <entt/entt.hpp>

namespace Systems {
    // Applies inputDir * speed to velocity, then velocity * dt to position.
    // Runs on whichever registry is passed in.
    void MovementSystem(entt::registry& registry, float dt);
}

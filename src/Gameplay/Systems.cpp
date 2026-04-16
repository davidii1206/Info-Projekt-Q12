/**
 * @file Systems.cpp
 * @brief Implementation of various entity-component systems.
 */

#include "Systems.h"
#include "Components.h"
#include <glm/glm.hpp>

/**
 * @brief Performs 3D movement system update.
 * 
 * Iterates over entities with TransformComponent and MovementComponent,
 * updating velocity based on input and position based on velocity.
 * 
 * @param registry The entt registry to operate on.
 * @param dt Delta time for the current frame.
 */
void Systems::MovementSystem(entt::registry& registry, float dt) {
    auto view = registry.view<TransformComponent, MovementComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& movement  = view.get<MovementComponent>(entity);

        // 3D movement update
        movement.velocity = movement.inputDir * movement.speed;
        transform.position += movement.velocity * dt;
    }
}

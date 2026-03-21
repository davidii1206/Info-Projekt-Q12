#include "Systems.h"
#include "Components.h"
#include <glm/glm.hpp>

void Systems::MovementSystem(entt::registry& registry, float dt) {
    auto view = registry.view<TransformComponent, MovementComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& movement  = view.get<MovementComponent>(entity);

        // XZ-plane movement; Y stays untouched (no jumping in prototype)
        movement.velocity = glm::vec3(movement.inputDir.x, 0.f, movement.inputDir.y) * movement.speed;
        transform.position += movement.velocity * dt;
    }
}

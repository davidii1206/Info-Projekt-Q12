/**
 * @file Systems.cpp
 * @brief Implementation of various entity-component systems.
 */

#include "Systems.h"
#include "Components.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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
    auto view = registry.view<TransformComponent, MovementComponent>(entt::exclude<PhysicsBodyComponent>);
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& movement  = view.get<MovementComponent>(entity);

        // 3D movement update
        movement.velocity = movement.inputDir * movement.speed;
        transform.position += movement.velocity * dt;
    }
}

void Systems::PhysicsSyncSystem(entt::registry& registry, PhysicsServer& physicsServer) {
    auto& bodyInterface = physicsServer.GetSystem().GetBodyInterface();

    auto view = registry.view<TransformComponent, PhysicsBodyComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& physComp = view.get<PhysicsBodyComponent>(entity);

        if (!physComp.handle.IsValid()) continue;

        // Fetch authoritative position/rotation from Jolt
        JPH::RVec3 joltPos = bodyInterface.GetPosition(physComp.handle.id);
        JPH::Quat joltRot = bodyInterface.GetRotation(physComp.handle.id);

        // Sync back to ECS transform
        transform.position = glm::vec3(joltPos.GetX(), joltPos.GetY(), joltPos.GetZ());
        
        // Convert Jolt rotation (Quat) to Euler degrees
        JPH::Vec3 euler = joltRot.GetEulerAngles();
        transform.rotation = glm::vec3(
            glm::degrees(euler.GetX()),
            glm::degrees(euler.GetY()),
            glm::degrees(euler.GetZ())
        );
    }
}
/**
 * @file Systems.h
 * @brief Header for various entity-component systems.
 */

#pragma once
#include <entt/entt.hpp>
#include "../Core/PhysicsServer.h"

/**
 * @namespace Systems
 * @brief Contains various entity-component system functions.
 */
namespace Systems {
    /**
     * @brief Applies input velocity to transform position.
     * 
     * Applies inputDir * speed to velocity, then velocity * dt to position.
     * Runs on whichever registry is passed in.
     * 
     * @param registry The entt registry to operate on.
     * @param dt Delta time for the current frame.
     */
    void MovementSystem(entt::registry& registry, float dt);

    /**
     * @brief Synchronizes EnTT Transform components with Jolt Physics bodies.
     * @param registry The server's EnTT registry.
     * @param physicsServer The active physics server.
     */
    void PhysicsSyncSystem(entt::registry& registry, PhysicsServer& physicsServer);
}
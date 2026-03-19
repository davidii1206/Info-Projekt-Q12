#pragma once
#include <entt/entt.hpp>

/**
 * @class World
 * @brief Manages the game state and all active entities.
 * 
 * The World class acts as a container for the ECS (Entity Component System) 
 * registry and orchestrates the updating of game logic.
 */
class World {
public:
    /**
     * @brief Initializes the game world and ECS systems.
     */
    World();

    /**
     * @brief Cleans up the world and destroys all entities.
     */
    ~World();

    /**
     * @brief Updates all game logic for the current frame.
     * @param dt The time elapsed since the last frame in seconds.
     */
    void Update(float dt);

private:
    /** @brief The EnTT registry holding all entities and components. */
    entt::registry m_Registry;
};
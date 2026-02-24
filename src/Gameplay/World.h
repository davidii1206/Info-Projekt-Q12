#pragma once
#include <entt/entt.hpp>

class World {
public:
    World();
    ~World();

    void Update(float dt);

private:
    entt::registry m_Registry;
};
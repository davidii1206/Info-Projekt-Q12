#pragma once
#include <glm/glm.hpp>
#include <cstdint>

enum class LightType : uint32_t {
    Directional = 0,
    Point = 1,
    Spot = 2
};

/**
 * @struct Light
 * @brief 16-byte aligned light data for GPU storage.
 */
struct Light {
    glm::vec4 position_type;   // xyz: position, w: type
    glm::vec4 direction_range;  // xyz: direction, w: range
    glm::vec4 color_intensity;  // xyz: color, w: intensity
};

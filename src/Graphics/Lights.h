/**
 * @file Lights.h
 * @brief Light data structures for the graphics system.
 */
#pragma once
#include <glm/glm.hpp>
#include <cstdint>

/**
 * @enum LightType
 * @brief Enumeration of supported light types.
 */
enum class LightType : uint32_t {
    Directional = 0, /**< Infinite directional light (e.g., Sun). */
    Point = 1,       /**< Point light emitting in all directions. */
    Spot = 2         /**< Focused cone light. */
};

/**
 * @struct Light
 * @brief 16-byte aligned light data for GPU storage.
 * 
 * This structure is designed to be passed to shaders in uniform or storage buffers.
 */
struct Light {
    glm::vec4 position_type;   /**< xyz: position, w: type (LightType). */
    glm::vec4 direction_range;  /**< xyz: direction, w: range. */
    glm::vec4 color_intensity;  /**< xyz: color, w: intensity. */
};
/**
 * @struct LightComponent
 * @brief ECS component for entities that emit light.
 */
struct LightComponent {
    LightType type      = LightType::Point;   ///< Light type (directional/point/spot).
    glm::vec3 color     = {1.0f, 1.0f, 1.0f}; ///< Light colour (linear RGB).
    float     intensity = 1.0f;               ///< Brightness multiplier.
    float     range     = 10.0f;              ///< Effective range (for point/spot).
    glm::vec3 direction = {0.0f, -1.0f, 0.0f}; ///< Direction (for directional/spot).

    LightComponent() = default; ///< Default constructor.
    /// @brief Constructs a light with explicit parameters.
    LightComponent(LightType t, glm::vec3 c, float i, float r,
                   glm::vec3 d = {0.f, -1.f, 0.f})
        : type(t), color(c), intensity(i), range(r), direction(d) {}
};

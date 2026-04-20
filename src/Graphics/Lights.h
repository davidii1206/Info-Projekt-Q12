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

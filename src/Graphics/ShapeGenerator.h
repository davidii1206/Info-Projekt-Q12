#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

/**
 * @struct ShapeVertex
 * @brief Standard vertex layout for basic geometric shapes.
 */
struct ShapeVertex {
    glm::vec3 position; ///< x, y, z coordinates.
    glm::vec4 color;    ///< r, g, b, a normalized (0.0 to 1.0).
};

/**
 * @struct ShapeData
 * @brief Container for generated geometry data.
 */
struct ShapeData {
    std::vector<ShapeVertex> vertices;
    std::vector<uint32_t> indices;
};

/**
 * @class ShapeGenerator
 * @brief Static utility for generating procedural geometric primitives.
 */
class ShapeGenerator {
public:
    /**
     * @brief Generates a simple 2D triangle.
     * @param size The height/width scale of the triangle.
     * @param color The color applied to all vertices.
     * @return ShapeData containing vertices and indices.
     */
    static ShapeData CreateTriangle(float size, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f});

    /**
     * @brief Generates a 3D unit cube.
     * @param size The side length of the cube.
     * @param color The color applied to all vertices.
     * @return ShapeData containing vertices and indices.
     */
    static ShapeData CreateCube(float size, const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f});
};

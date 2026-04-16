/**
 * @file ShapeGenerator.cpp
 * @brief Implementation of the ShapeGenerator utility for procedural geometry.
 */
#include "ShapeGenerator.h"

ShapeData ShapeGenerator::CreateTriangle(float size, const glm::vec4& color) {
    ShapeData data;
    float half = size * 0.5f;

    // Define vertices for a simple triangle
    data.vertices = {
        { { 0.0f,  half, 0.0f }, color },
        { { half, -half, 0.0f }, color },
        { {-half, -half, 0.0f }, color }
    };

    data.indices = { 0, 1, 2 };

    return data;
}

ShapeData ShapeGenerator::CreateCube(float size, const glm::vec4& color) {
    ShapeData data;
    float h = size * 0.5f;

    // Define vertices for a unit cube
    data.vertices = {
        // Front
        { {-h, -h,  h}, color }, { { h, -h,  h}, color }, { { h,  h,  h}, color }, { {-h,  h,  h}, color },
        // Back
        { {-h, -h, -h}, color }, { { h, -h, -h}, color }, { { h,  h, -h}, color }, { {-h,  h, -h}, color }
    };

    // Define indices for the cube's triangles
    data.indices = {
        // front
        0, 1, 2, 2, 3, 0,
        // top
        3, 2, 6, 6, 7, 3,
        // back
        7, 6, 5, 5, 4, 7,
        // bottom
        4, 5, 1, 1, 0, 4,
        // left
        4, 0, 3, 3, 7, 4,
        // right
        1, 5, 6, 6, 2, 1
    };

    return data;
}

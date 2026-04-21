/**
 * @file GlobalUniforms.h
 * @brief Shared uniform data structures for rendering.
 */
#pragma once
#include <glm/glm.hpp>
#include "Lights.h"

/**
 * @struct GlobalUniforms
 * @brief Standardized uniform buffer for shared scene data.
 * 
 * Grouped into vec4s to ensure perfect 16-byte alignment across all platforms.
 * This structure is uploaded once per frame and accessed by various shaders.
 */
struct alignas(16) GlobalUniforms {
    glm::mat4 view;         /**< View matrix. */
    glm::mat4 proj;         /**< Projection matrix. */
    glm::mat4 viewProj;     /**< View-Projection matrix. */
    glm::mat4 sunVP;        /**< Sun's View-Projection matrix for shadows. */
    glm::vec4 ambientColor;

    glm::vec4 sunColor;     /**< Sun light color and intensity. */
    glm::vec4 sunDir;       /**< Sun light direction. */

    glm::vec4 cameraPos;    /**< Camera position in world space. */
    
    /** @brief Timer data: x=time, y=numLights, z=deltaTime, w=frameCount. */
    glm::vec4 timers;       
    
    /** @brief Screen data: xy=resolution, z=posterizeSteps, w=padding. */
    glm::vec4 screen;       
    
    Light lights[16];       /**< Array of dynamic scene lights. */
};

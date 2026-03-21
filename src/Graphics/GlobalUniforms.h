#pragma once
#include <glm/glm.hpp>
#include "Lights.h"

/**
 * @struct GlobalUniforms
 * @brief Standardized uniform buffer for shared scene data.
 * 
 * Grouped into vec4s to ensure perfect 16-byte alignment across all platforms.
 */
struct alignas(16) GlobalUniforms {
    glm::mat4 view;         // 0
    glm::mat4 proj;         // 64
    glm::mat4 viewProj;     // 128
    glm::mat4 sunVP;        // 192

    glm::vec4 sunColor;     // 256
    glm::vec4 sunDir;       // 272

    glm::vec4 cameraPos;    // 288
    
    // x: time, y: numLights, z: deltaTime, w: frameCount
    glm::vec4 timers;       // 304
    
    // xy: resolution, z: posterizeSteps, w: padding
    glm::vec4 screen;       // 320
    
    Light lights[16];       // 336
};

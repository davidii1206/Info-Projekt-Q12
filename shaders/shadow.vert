#version 450

layout(location = 0) in vec3 inPosition;
// Other attributes ignored — depth only

// Set 1: Per-draw push constants (Slot 0: model matrix, Slot 1: sunVP)
layout(set = 1, binding = 0) uniform ShadowPC {
    mat4 model;
} pc;

layout(set = 1, binding = 1) uniform ShadowVP {
    mat4 sunVP;
} vp;

void main() {
    gl_Position = vp.sunVP * pc.model * vec4(inPosition, 1.0);
}

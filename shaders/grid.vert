#version 450

struct Light {
    vec4 position_type;
    vec4 direction_range;
    vec4 color_intensity;
};

layout(location = 0) in vec3 inPosition;

// Use layout(set=0) to match BindVertexStorageBuffer(0, GetGlobalUBO())
layout(set = 0, binding = 0, std430) readonly buffer GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 sunVP;
    vec4 sunColor;
    vec4 sunDir;
    vec4 cameraPos;
    vec4 ambientColor;
    vec4 timers;
    vec4 screen;
    Light lights[16];
} globals;

// SDL3 GPU: Uniform Buffers (push constants) are set 1
layout(set = 1, binding = 0) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) out vec3 vWorldPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = globals.viewProj * worldPos;

    if (any(isnan(gl_Position)) || any(isinf(gl_Position)))
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

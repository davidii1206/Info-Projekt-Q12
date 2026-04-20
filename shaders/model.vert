#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec3 inTangent;

struct Light {
    vec4 position_type; // xyz: position, w: type
    vec4 direction_range; // xyz: direction, w: range
    vec4 color_intensity; // xyz: color, w: intensity
};

// SDL3 GPU SPIR-V Conventions (4-set layout):
// Set 0: Vertex Samplers / Storage Buffers
// Set 1: Vertex Uniform Buffers
// Set 2: Fragment Samplers / Storage Buffers
// Set 3: Fragment Uniform Buffers

// Slot 0 (pc) -> Binding 0 in Set 1
layout(set = 1, binding = 0) uniform PushConstants {
    mat4 model;
} pc;

// Slot 0 (globals) -> Binding 0 in Set 0 (0 samplers before it)
layout(set = 0, binding = 0, std430) readonly buffer GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 sunVP;
    vec4 sunColor;
    vec4 sunDir;
    vec4 cameraPos;
    vec4 timers; // x: time, y: numLights, z: deltaTime, w: frameCount
    vec4 screen; // xy: resolution, zw: padding
    Light lights[16];
} globals;


layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vTexCoords;
layout(location = 2) out vec4 vColor;
layout(location = 3) out vec3 vPos;

bool is_nan(mat4 m) {
    return any(isnan(m[0])) || any(isnan(m[1])) || any(isnan(m[2])) || any(isnan(m[3]));
}

void main() {
    mat4 model = pc.model;
    if (is_nan(model)) {
        model = mat4(1.0);
    }

    mat4 viewProj = globals.viewProj;
    if (is_nan(viewProj)) {
        viewProj = mat4(1.0);
    }

    vec4 worldPos = model * vec4(inPosition, 1.0);
    vPos = worldPos.xyz;
    gl_Position = viewProj * worldPos;
    
    // Fallback for extreme values or NaN in final position
    if (any(isnan(gl_Position)) || any(isinf(gl_Position))) {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    }

    vNormal = normalize(mat3(model) * inNormal);
    if (any(isnan(vNormal))) {
        vNormal = inNormal;
    }

    vTexCoords = inTexCoords;
    vColor = inColor;
}

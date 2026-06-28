#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec3 inTangent;

struct Light {
    vec4 position_type;   // xyz: position, w: type
    vec4 direction_range; // xyz: direction, w: range
    vec4 color_intensity; // xyz: color, w: intensity
};

// Slot 0 (globals) -> Binding 0 in Set 0
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

// Slot 1 (instanceBuf) -> Binding 1 in Set 0
layout(set = 0, binding = 1, std430) readonly buffer InstanceTransforms {
    mat4 transforms[];
} instanceBuf;

// Push constants (Slot 0, Binding 0 in Set 1 – wird als Uniform Buffer behandelt)
layout(set = 1, binding = 0) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vTexCoords;
layout(location = 2) out vec4 vColor;
layout(location = 3) out vec3 vPos;

bool is_nan(mat4 m) {
    return any(isnan(m[0])) || any(isnan(m[1])) || any(isnan(m[2])) || any(isnan(m[3]));
}

void main() {
    mat4 model = pc.model * instanceBuf.transforms[gl_InstanceIndex];
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
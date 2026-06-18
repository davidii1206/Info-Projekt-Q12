#version 450

layout(location = 0) in vec3 vWorldPos;

struct Light {
    vec4 position_type;
    vec4 direction_range;
    vec4 color_intensity;
};

// Use layout(set=0) to match BindFragmentStorageBuffer(0, GetGlobalUBO())
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

layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outLight;
layout(location = 3) out uint outID;

vec3 computeLighting(vec3 fragPos, vec3 normal) {
    vec3 lighting = vec3(0.0);
    
    // Sun light
    vec3 sunDir = normalize(globals.sunDir.xyz);
    float diff = max(dot(normal, sunDir), 0.0);
    lighting += diff * globals.sunColor.rgb * globals.sunColor.w;

    lighting += globals.ambientColor.rgb * globals.ambientColor.w;
    return lighting;
}

void main() {
    outNormal = vec4(0.0, 1.0, 0.0, 0.5);
    outColor = vec4(0.2, 0.8, 0.2, 0.5); // Ghost color

    vec3 normal = vec3(0.0, 1.0, 0.0);
    outLight.rgb = computeLighting(vWorldPos, normal);
    outLight.a = 0.5;
    outID = 0u;
}

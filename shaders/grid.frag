#version 450

struct Light {
    vec4 position_type;
    vec4 direction_range;
    vec4 color_intensity;
};

layout(location = 0) in vec3 vWorldPos;

layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outLight;
layout(location = 3) out uint outID;

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

void main() {
    const float gridSize = 2.0;

    // Square grid: compute coordinates aligned with snapping
    vec2 coord = vWorldPos.xz;
    vec2 gridUV = coord / gridSize;

    // Anti-aliased grid lines (centered between integers)
    vec2 grid = abs(fract(gridUV) - 0.5);
    vec2 d = fwidth(gridUV);
    vec2 line = smoothstep(vec2(0.0), d * 1.5, grid);
    float pattern = min(line.x, line.y);

    // Grid color: light grey lines
    vec3 gridColor = mix(vec3(0.4, 0.4, 0.4), vec3(0.0), pattern);

    // Distance fade (extended for better visibility in building mode)
    float dist = length(vWorldPos.xz - globals.cameraPos.xyz.xz);
    float fade = 1.0 - smoothstep(40.0, 100.0, dist);

    // Semi-transparent lines (0.7), transparent squares (0.0)
    float alpha = mix(0.7, 0.0, pattern) * fade;

    // Simple ground lighting (ambient + sun diffuse)
    vec3 normal = vec3(0.0, 1.0, 0.0);
    vec3 lightDir = normalize(-globals.sunDir.xyz);
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 ambient = globals.ambientColor.rgb * globals.ambientColor.a;
    vec3 lighting = ambient + globals.sunColor.rgb * globals.sunColor.a * NdotL;

    outNormal = vec4(normal * 0.5 + 0.5, alpha);
    outColor  = vec4(gridColor, alpha);
    outLight  = vec4(lighting, alpha);
    outID     = 0;
}

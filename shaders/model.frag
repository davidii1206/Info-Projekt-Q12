#version 450

layout(location = 0) in vec3 outNormal;
layout(location = 1) in vec2 outTexCoords;
layout(location = 2) in vec4 outColor;
layout(location = 3) in vec3 outPos;
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

struct GPUMaterial {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int baseColorTextureIndex;
    int normalTextureIndex;
};

// Fragment Samplers are in Set 2. Slot 0 -> Binding 0.
layout(set = 2, binding = 0) uniform sampler2D baseColorTexture;

// Binding 1: Global Uniforms (Slot 0) in Set 2 (Offset by 1 sampler)
layout(set = 2, binding = 1, std430) readonly buffer GlobalUniforms {
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

// Binding 2: Material Buffer (Slot 1) in Set 2 (Offset by 1 sampler)
layout(set = 2, binding = 2, std430) readonly buffer MaterialBuffer {
    GPUMaterial materials[];
} matBuffer;

// Slot 0 (pc) -> Binding 0 in Set 3
layout(set = 3, binding = 0) uniform MaterialIndex {
    vec4 materialIndex; // x = index, rest is padding
} pc;

layout(location = 0) out vec4 fragColor;

void main() {
    uint matIdx = uint(pc.materialIndex.x);
    GPUMaterial mat = matBuffer.materials[matIdx];
    
    vec4 texColor = texture(baseColorTexture, outTexCoords);
    vec4 baseColor = mat.baseColorFactor * outColor * texColor;
    
    if (baseColor.a < 0.1) discard;

    vec3 N = normalize(outNormal);
    vec3 V = normalize(globals.cameraPos.xyz - outPos);
    
    vec3 totalDiffuse = vec3(0.1); // Ambient
    
    uint numLights = uint(globals.timers.y);
    for (uint i = 0; i < numLights; ++i) {
        Light light = globals.lights[i];
        
        vec3 L;
        float attenuation = 1.0;
        float intensity = light.color_intensity.w;
        vec3 lightColor = light.color_intensity.rgb;
        
        int type = int(light.position_type.w);
        
        if (type == 0) { // Directional
            L = normalize(-light.direction_range.xyz);
        } else { // Point or Spot
            vec3 lightDir = light.position_type.xyz - outPos;
            float distance = length(lightDir);
            L = normalize(lightDir);
            
            float range = light.direction_range.w;
            attenuation = max(0.0, 1.0 - (distance / range));
            attenuation *= attenuation; // Quadratic falloff for smoother look
            
            if (type == 2) { // Spot
                float theta = dot(L, normalize(-light.direction_range.xyz));
                float innerCutoff = 0.9; // Hardcoded for now
                float outerCutoff = 0.8;
                float epsilon = innerCutoff - outerCutoff;
                float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
                attenuation *= spotIntensity;
            }
        }
        
        float diff = max(dot(N, L), 0.0);
        totalDiffuse += lightColor * intensity * diff * attenuation;
    }
    
    fragColor = vec4(baseColor.rgb * totalDiffuse, baseColor.a);
}

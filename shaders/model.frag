#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vTexCoords;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec3 vPos;
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
    uint materialIndex;
    uint objectID;
} pc;

layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outLight;
layout(location = 3) out uint outID;

void main() {
    uint matIdx = uint(pc.materialIndex.x);
    GPUMaterial mat = matBuffer.materials[matIdx];
    
    vec4 texColor = texture(baseColorTexture, vTexCoords);
    vec4 baseColor = mat.baseColorFactor * vColor * texColor;
    
    if (baseColor.a < 0.1) discard;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(globals.cameraPos.xyz - vPos);
    
    vec3 totalDiffuse = vec3(0.1); // Ambient
    
    uint numLights = uint(globals.timers.y);
    float posterizeSteps = globals.screen.z;

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
            vec3 lightDir = light.position_type.xyz - vPos;
            float distance = length(lightDir);
            L = normalize(lightDir);
            
            float range = light.direction_range.w;
            attenuation = max(0.0, 1.0 - (distance / range));
            
            if (type == 2) { // Spot
                float theta = dot(L, normalize(-light.direction_range.xyz));
                float innerCutoff = 0.95; 
                float outerCutoff = 0.85;
                float epsilon = innerCutoff - outerCutoff;
                float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
                attenuation *= spotIntensity;
            }
        }
        
        float diff = max(dot(N, L), 0.0);
        float lightIntensity = diff * attenuation;

        // Apply Toon/Posterization to the light intensity directly
        if (posterizeSteps > 0.0) {
            lightIntensity = floor(lightIntensity * posterizeSteps) / posterizeSteps;
        }

        totalDiffuse += lightColor * intensity * lightIntensity;
    }
    
    outNormal = vec4(N * 0.5 + 0.5, 1.0);
    outColor = baseColor;
    outLight = vec4(totalDiffuse, 1.0);
}

#version 450

// ---------------------------------------------------------------------------
// Inputs from vertex shader
// ---------------------------------------------------------------------------
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vTexCoords;
layout(location = 2) in vec4 vColor;
layout(location = 3) in vec3 vPos;

// ---------------------------------------------------------------------------
// Light struct (must match GlobalUniforms.lights layout in C++)
// ---------------------------------------------------------------------------
struct Light {
    vec4 position_type;   // xyz: world position, w: type (0=Dir, 1=Point, 2=Spot)
    vec4 direction_range; // xyz: direction (normalized), w: range
    vec4 color_intensity; // xyz: linear RGB color, w: intensity
};

// ---------------------------------------------------------------------------
// Material struct (must match GPUMaterial in C++)
// ---------------------------------------------------------------------------
struct GPUMaterial {
    vec4  baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int   baseColorTextureIndex;
    int   normalTextureIndex;
};

// ---------------------------------------------------------------------------
// SDL3 GPU SPIR-V binding conventions:
//   Set 2  = Fragment Samplers + Storage Buffers
//   Set 3  = Fragment Uniform Buffers
// ---------------------------------------------------------------------------

// Binding 0: base color texture (1 sampler before storage buffers)
layout(set = 2, binding = 0) uniform sampler2D baseColorTexture;

// Binding 1: GlobalUniforms SSBO (offset by 1 sampler)
layout(set = 2, binding = 1, std430) readonly buffer GlobalUniforms {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 sunVP;
    vec4 sunColor;      // xyz: color,  w: intensity
    vec4 sunDir;        // xyz: direction (points TOWARD the sun), w: unused
    vec4 cameraPos;     // xyz: world-space camera pos
    vec4 ambientColor;  // xyz: ambient color, w: ambient intensity
    vec4 timers;        // x: time, y: numLights, z: deltaTime, w: frameCount
    vec4 screen;        // xy: resolution, z: posterizeSteps, w: unused
    Light lights[16];
} globals;

// Binding 2: Material buffer SSBO
layout(set = 2, binding = 2, std430) readonly buffer MaterialBuffer {
    GPUMaterial materials[];
} matBuffer;

// Fragment push constants: material index + object ID
layout(set = 3, binding = 0) uniform MaterialIndex {
    uint materialIndex;
    uint objectID;
} pc;

// ---------------------------------------------------------------------------
// Outputs (G-Buffer)
// ---------------------------------------------------------------------------
layout(location = 0) out vec4 outNormal; // RGB: packed normal,  A: 1
layout(location = 1) out vec4 outColor;  // RGBA: albedo
layout(location = 2) out vec4 outLight;  // RGB: accumulated light contribution
layout(location = 3) out uint outID;     // object picking ID

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
const float SHININESS      = 32.0;   // Specular shininess (higher = tighter highlight)
const float SPECULAR_POWER = 0.5;    // How strong specular looks (0–1)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Computes Blinn-Phong shading for a single light contribution.
 *
 * @param L          Normalized light direction (fragment -> light).
 * @param N          Normalized surface normal.
 * @param V          Normalized view direction (fragment -> camera).
 * @param attenuation  Range/angle attenuation factor (0–1).
 * @param lightColor   Linear RGB color of the light.
 * @param intensity    Intensity multiplier.
 * @return vec3 combined diffuse + specular contribution.
 */
vec3 BlinnPhong(vec3 L, vec3 N, vec3 V,
                float attenuation,
                vec3 lightColor, float intensity)
{
    // --- Diffuse (Lambertian) ---
    float NdotL = max(dot(N, L), 0.0);

    // --- Specular (Blinn-Phong half-vector) ---
    vec3  H      = normalize(L + V);
    float NdotH  = max(dot(N, H), 0.0);
    float spec   = pow(NdotH, SHININESS) * SPECULAR_POWER;

    vec3 contribution = lightColor * intensity * attenuation * (NdotL + spec);
    return contribution;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main() {
    // -- Material & Texture --
    GPUMaterial mat = matBuffer.materials[pc.materialIndex];
    vec4 texColor   = texture(baseColorTexture, vTexCoords);
    vec4 albedo     = mat.baseColorFactor * vColor * texColor;
    if (albedo.a < 0.1) discard;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(globals.cameraPos.xyz - vPos);

    float posterizeSteps = globals.screen.z;

    // ----------------------------------------------------------------
    // 1. AMBIENT
    // ----------------------------------------------------------------
    vec3 ambient = globals.ambientColor.rgb * globals.ambientColor.w;

    // ----------------------------------------------------------------
    // 2. SUN (directional, always active)
    //    sunDir stores the direction pointing TOWARD the sun.
    // ----------------------------------------------------------------
    vec3 totalLight = ambient;
    {
        vec3  sunL     = normalize(globals.sunDir.xyz);
        float sunInten = globals.sunColor.w;
        vec3  sunCol   = globals.sunColor.rgb;
        totalLight    += BlinnPhong(sunL, N, V, 1.0, sunCol, sunInten);
    }

    // ----------------------------------------------------------------
    // 3. DYNAMIC LIGHTS (from ECS LightComponents)
    // ----------------------------------------------------------------
    uint numLights = uint(globals.timers.y);

    for (uint i = 0u; i < numLights; ++i) {
        Light light = globals.lights[i];
        int   type  = int(light.position_type.w);

        vec3  L           = vec3(0.0);
        float attenuation = 1.0;
        float intensity   = light.color_intensity.w;
        vec3  lightColor  = light.color_intensity.rgb;

        if (type == 0) {
            // --- Directional ---
            L = normalize(-light.direction_range.xyz);
        }
        else if (type == 1) {
            // --- Point ---
            vec3  toLight  = light.position_type.xyz - vPos;
            float dist     = length(toLight);
            float range    = light.direction_range.w;
            L              = normalize(toLight);
            // Smooth quadratic falloff clamped to range
            float ratio    = clamp(dist / range, 0.0, 1.0);
            attenuation    = 1.0 - ratio * ratio;
        }
        else if (type == 2) {
            // --- Spot ---
            vec3  toLight  = light.position_type.xyz - vPos;
            float dist     = length(toLight);
            float range    = light.direction_range.w;
            L              = normalize(toLight);

            // Distance falloff (same as point)
            float ratio    = clamp(dist / range, 0.0, 1.0);
            attenuation    = 1.0 - ratio * ratio;

            // Cone falloff
            // innerCutoff / outerCutoff are stored packed in the direction vec's w.
            // We use fixed defaults here to match LightComponent defaults;
            // for per-light cutoffs, pack them into an unused channel.
            float innerCutoff = 0.95;
            float outerCutoff = 0.85;
            float theta       = dot(L, normalize(-light.direction_range.xyz));
            float epsilon     = innerCutoff - outerCutoff;
            float spotFactor  = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
            attenuation      *= spotFactor;
        }

        // Posterize / toon-shade the per-light intensity
        float lightBrightness = max(dot(N, L), 0.0) * attenuation;
        if (posterizeSteps > 0.0) {
            lightBrightness = floor(lightBrightness * posterizeSteps) / posterizeSteps;
            // Rebuild a toon-shaded contribution without specular (stylized look)
            totalLight += lightColor * intensity * lightBrightness;
        } else {
            totalLight += BlinnPhong(L, N, V, attenuation, lightColor, intensity);
        }
    }

    // ----------------------------------------------------------------
    // Output to G-Buffer
    // ----------------------------------------------------------------
    outNormal = vec4(N * 0.5 + 0.5, 1.0);
    outColor  = albedo;
    outLight  = vec4(totalLight, 1.0);
    outID     = pc.objectID;
}

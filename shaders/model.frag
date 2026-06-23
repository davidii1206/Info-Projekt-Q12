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

// Binding 0: base color texture
layout(set = 2, binding = 0) uniform sampler2D baseColorTexture;

// Binding 1 (sampler): shadow map depth texture
layout(set = 2, binding = 1) uniform sampler2DShadow shadowMap;

// Binding 2: GlobalUniforms SSBO (offset by 2 samplers)
layout(set = 2, binding = 2, std430) readonly buffer GlobalUniforms {
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

// Binding 3: Material buffer SSBO
layout(set = 2, binding = 3, std430) readonly buffer MaterialBuffer {
    GPUMaterial materials[];
} matBuffer;

// Fragment push constants: material index, object ID, alpha, blocksView flag,
// ghost position (for tree fade), and tint colour.
// vec4 used throughout so std140 alignment is 1:1 with the C++ FragPC struct.
layout(set = 3, binding = 0) uniform MaterialIndex {
    uint  materialIndex;
    uint  objectID;
    float alpha;
    float blocksView;     // >0 = apply distance-based transparency near ghostPos
    vec4  ghostPos;       // xyz = fade centre when blocksView > 0, w unused
    vec4  tintColor;      // rgb = per-object colour tint (1 = no tint), w unused
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
// PCF Shadow Sampling
// ---------------------------------------------------------------------------

/**
 * Samples the shadow map with a 5x5 PCF kernel and slope-scale bias.
 *
 * @param shadowCoord  Fragment position in sun clip space (xyz/w = NDC).
 * @param N            Surface normal (world space), used for slope-scale bias.
 * @return             Shadow factor: 1.0 = fully lit, 0.0 = fully shadowed.
 */
float SampleShadowPCF(vec4 shadowCoord, vec3 N) {
    // Perspective divide
    vec3 proj = shadowCoord.xyz / shadowCoord.w;

    // NDC [-1,1] -> UV [0,1]
    // Note: SDL3 GPU uses +Y up in NDC but +Y down in UV space (top-left origin).
    // Thus, Y must be flipped: uv.y = -proj.y * 0.5 + 0.5.
    vec2 uv = vec2(proj.x * 0.5 + 0.5, proj.y * -0.5 + 0.5);
    float depth = proj.z;

    // Fragments outside the shadow frustum are fully lit — no shadow
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        depth < 0.0 || depth > 1.0)
        return 1.0;

    // Slope-scale bias: grazing angles need more bias to avoid acne.
    float NdotL_sun = clamp(dot(N, normalize(globals.sunDir.xyz)), 0.0, 1.0);
    float slopeBias = mix(0.001, 0.0002, NdotL_sun);

    // 5x5 PCF kernel — soft shadow edges
    vec2  texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    float shadow    = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(shadowMap, vec3(uv + offset, depth - slopeBias));
        }
    }

    return shadow / 25.0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main() {
    // -- Material & Texture --
    GPUMaterial mat = matBuffer.materials[pc.materialIndex];
    vec4 texColor   = texture(baseColorTexture, vTexCoords);
    vec4 albedo     = mat.baseColorFactor * vColor * texColor;
    albedo.a *= pc.alpha;

    // Distance-based tree fade: when blocksView > 0, tall assets near the
    // cursor position fade out so the player can see the placement area.
    // smoothstep(5, 35, dist) gives full transparency within ~5 units of the
    // cursor, fully opaque beyond ~35 units, with a smooth falloff between.
    if (pc.blocksView > 0.5) {
        float dist = length(vPos.xz - pc.ghostPos.xz);
        float fade = smoothstep(5.0, 35.0, dist);
        albedo.a *= fade;
    }

    albedo.rgb *= pc.tintColor.rgb;

    if (albedo.a < 0.01) discard;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(globals.cameraPos.xyz - vPos);

    float posterizeSteps = globals.screen.z;

    // ----------------------------------------------------------------
    // 1. AMBIENT
    // ----------------------------------------------------------------
    vec3 ambient = globals.ambientColor.rgb * globals.ambientColor.w;

    // ----------------------------------------------------------------
    // 2. SUN (directional, always active)
    //    sunDir already points TOWARD the sun (set in GameScene.cpp as
    //    normalize(-m_SunDirection)), so use it directly — no negation.
    // ----------------------------------------------------------------
    vec3 totalLight = ambient;
    {
        vec3  sunL     = normalize(globals.sunDir.xyz); // FIX: was normalize(-globals.sunDir.xyz) — double negation
        float sunInten = globals.sunColor.w;
        vec3  sunCol   = globals.sunColor.rgb;

        // PCF soft shadow — transform fragment to sun clip space
        float NdotSun  = dot(N, sunL);
        // Surfaces facing away from the sun get shadowFactor = 0 (fully shadowed).
        // Smoothstep over [0, 0.1] creates a soft terminator instead of a hard black edge.
        float shadowFactor = (NdotSun <= 0.0)
            ? 0.0
            : SampleShadowPCF(globals.sunVP * vec4(vPos, 1.0), N) * smoothstep(0.0, 0.1, NdotSun);

        // Posterize / toon-shade the sun intensity
        float lightBrightness = max(NdotSun, 0.0) * shadowFactor;
        if (posterizeSteps > 0.0) {
            lightBrightness = floor(lightBrightness * posterizeSteps) / posterizeSteps;
            totalLight += sunCol * sunInten * lightBrightness;
        } else {
            totalLight += BlinnPhong(sunL, N, V, shadowFactor, sunCol, sunInten);
        }
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

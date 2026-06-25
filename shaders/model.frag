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
//   Set 3  = Fragment Uniform Buffers (Push Constants)
// ---------------------------------------------------------------------------

// Binding 0: base color texture (wird in C++ dynamisch mit Building‑Textur überschrieben)
layout(set = 2, binding = 0) uniform sampler2D baseColorTexture;

// Binding 1: shadow map
layout(set = 2, binding = 1) uniform sampler2DShadow shadowMap;

// Binding 2: per-tile fog of war texture
layout(set = 2, binding = 2) uniform sampler2D fogTexture;

// Binding 3: Bau‑Sprite‑Sheet (8×8 Frames)
layout(set = 2, binding = 3) uniform sampler2D constructionSheet;

// Binding 4: GlobalUniforms SSBO
layout(set = 2, binding = 4, std430) readonly buffer GlobalUniforms {
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

// Binding 5: Material buffer SSBO
layout(set = 2, binding = 5, std430) readonly buffer MaterialBuffer {
    GPUMaterial materials[];
} matBuffer;

// ---------------------------------------------------------------------------
// Fragment Push Constants – exakt passend zur C++‑Struktur `FragPC`
// ---------------------------------------------------------------------------
layout(set = 3, binding = 0) uniform FragPC {
    uint  matIdx;          // material index
    uint  objID;           // object ID für Picking
    float alpha;           // globale Deckkraft
    float blocksView;      // >0 = Baum‑Fade aktiv
    float fogEnabled;      // >0 = Fog‑of‑War aktiv
    float buildProgress;   // 0..1 Baufortschritt
    float damageState;     // 0=healthy, 1=damaged, 2=critical
    float tribeIndex;      // BugClass-Index für optionale Shader-Tints
    vec4  ghostPos;        // xyz = Fade‑Zentrum für Baum‑Fade
    vec4  tintColor;       // rgb = Farbton (1 = kein Tint)
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
const float SHININESS      = 32.0;
const float SPECULAR_POWER = 0.5;

// ---------------------------------------------------------------------------
// Hilfsfunktionen (unverändert)
// ---------------------------------------------------------------------------
vec3 BlinnPhong(vec3 L, vec3 N, vec3 V, float attenuation, vec3 lightColor, float intensity) {
    float NdotL = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float spec = pow(NdotH, SHININESS) * SPECULAR_POWER;
    return lightColor * intensity * attenuation * (NdotL + spec);
}

float SampleShadowPCF(vec4 shadowCoord, vec3 N) {
    vec3 proj = shadowCoord.xyz / shadowCoord.w;
    vec2 uv = vec2(proj.x * 0.5 + 0.5, proj.y * -0.5 + 0.5);
    float depth = proj.z;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth < 0.0 || depth > 1.0)
        return 1.0;
    float NdotL_sun = clamp(dot(N, normalize(globals.sunDir.xyz)), 0.0, 1.0);
    float slopeBias = mix(0.001, 0.0002, NdotL_sun);
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(shadowMap, vec3(uv + offset, depth - slopeBias));
        }
    }
    return shadow / 25.0;
}

// ---------------------------------------------------------------------------
// Hauptprogramm
// ---------------------------------------------------------------------------
void main() {
    // ---------- 1. Material & Basistextur ----------
    GPUMaterial mat = matBuffer.materials[pc.matIdx];
    vec4 texColor = texture(baseColorTexture, vTexCoords);
    vec4 albedo = mat.baseColorFactor * vColor * texColor;
    albedo.a *= pc.alpha;

    // ---------- 2. Baum‑Fade (für Scatter‑Bäume im Building‑Modus) ----------
    if (pc.blocksView > 0.5) {
        float dist = length(vPos.xz - pc.ghostPos.xz);
        float fade = smoothstep(5.0, 35.0, dist);
        albedo.a *= fade;
    }

    // ---------- 3. Bau‑Animation ----------
    float build = clamp(pc.buildProgress, 0.0, 1.0);
    if (build < 1.0) {
        // Sprite‑Sheet: 8×8 = 64 Frames
        float totalFrames = 64.0;
        float frameIndex = floor(build * totalFrames);
        float frameCol = mod(frameIndex, 8.0);
        float frameRow = floor(frameIndex / 8.0);
        vec2 frameUV = vec2(
            (vTexCoords.x + frameCol) / 8.0,
            (vTexCoords.y + frameRow) / 8.0
        );
        vec4 sheetColor = texture(constructionSheet, frameUV);

        // Mische: grünliche Einfärbung + Sprite‑Overlay
        vec4 buildColor = mix(vec4(0.2, 0.8, 0.2, 1.0), albedo, build);
        albedo = mix(buildColor, sheetColor, 0.5 * (1.0 - build));
        albedo.a = mix(0.0, 1.0, build);
        albedo.rgb *= pc.tintColor.rgb;
    } else {
        // Fertig gebaut: Textur-Alpha durch pc.alpha ersetzen (PNG-Sprites haben
        // transparenten Hintergrund der auf einem 3D-Cube falsch aussieht).
        if (pc.tribeIndex > 0.0) albedo.a = pc.alpha;

        // Schadens-Overlay + Tint (5 Stufen: 0=Healthy…4=Destroyed)
        float dmg = clamp(pc.damageState, 0.0, 4.0);
        if (dmg >= 1.0) {
            // leichter roter Stich ab Stufe 1
            float severity = dmg / 4.0;
            albedo.rgb = mix(albedo.rgb, albedo.rgb * vec3(0.55, 0.35, 0.35), severity * 0.5);
        }
        if (dmg >= 3.0) {
            // ab Critical: zusätzlich abdunkeln
            albedo.rgb *= mix(1.0, 0.65, (dmg - 3.0));
        }
        if (dmg >= 4.0) {
            // Destroyed: stark verdunkeln + Rauch-Grau-Tint
            albedo.rgb = mix(albedo.rgb, vec3(0.15, 0.12, 0.10), 0.55);
        }
        albedo.rgb *= pc.tintColor.rgb;
    }

    // ---------- 4. Fog of War ----------
    if (pc.fogEnabled > 0.5) {
        vec2 fogUV = (vPos.xz + vec2(375.0)) / 750.0;
        float vis = texture(fogTexture, fogUV).r;
        if (vis < 0.5) discard;
    }

    if (albedo.a < 0.01) discard;

    // ---------- 5. Beleuchtung ----------
    vec3 N = normalize(vNormal);
    vec3 V = normalize(globals.cameraPos.xyz - vPos);
    float posterizeSteps = globals.screen.z;

    // 5a. Ambient
    vec3 totalLight = globals.ambientColor.rgb * globals.ambientColor.w;

    // 5b. Sonne (directional)
    {
        vec3 sunL = normalize(globals.sunDir.xyz);
        float sunInten = globals.sunColor.w;
        vec3 sunCol = globals.sunColor.rgb;
        float NdotSun = dot(N, sunL);
        float shadowFactor = (NdotSun <= 0.0) ? 0.0 : SampleShadowPCF(globals.sunVP * vec4(vPos, 1.0), N) * smoothstep(0.0, 0.1, NdotSun);
        float lightBrightness = max(NdotSun, 0.0) * shadowFactor;
        if (posterizeSteps > 0.0) {
            lightBrightness = floor(lightBrightness * posterizeSteps) / posterizeSteps;
            totalLight += sunCol * sunInten * lightBrightness;
        } else {
            totalLight += BlinnPhong(sunL, N, V, shadowFactor, sunCol, sunInten);
        }
    }

    // 5c. Dynamische Lichter (Punkt‑, Spot‑, Richtungs‑)
    uint numLights = uint(globals.timers.y);
    for (uint i = 0u; i < numLights; ++i) {
        Light light = globals.lights[i];
        int type = int(light.position_type.w);
        vec3 L = vec3(0.0);
        float attenuation = 1.0;
        float intensity = light.color_intensity.w;
        vec3 lightColor = light.color_intensity.rgb;

        if (type == 0) {
            L = normalize(-light.direction_range.xyz);
        } else if (type == 1) {
            vec3 toLight = light.position_type.xyz - vPos;
            float dist = length(toLight);
            float range = light.direction_range.w;
            L = normalize(toLight);
            float ratio = clamp(dist / range, 0.0, 1.0);
            attenuation = 1.0 - ratio * ratio;
        } else if (type == 2) {
            vec3 toLight = light.position_type.xyz - vPos;
            float dist = length(toLight);
            float range = light.direction_range.w;
            L = normalize(toLight);
            float ratio = clamp(dist / range, 0.0, 1.0);
            attenuation = 1.0 - ratio * ratio;
            float innerCutoff = 0.95;
            float outerCutoff = 0.85;
            float theta = dot(L, normalize(-light.direction_range.xyz));
            float epsilon = innerCutoff - outerCutoff;
            float spotFactor = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
            attenuation *= spotFactor;
        }

        float lightBrightness = max(dot(N, L), 0.0) * attenuation;
        if (posterizeSteps > 0.0) {
            lightBrightness = floor(lightBrightness * posterizeSteps) / posterizeSteps;
            totalLight += lightColor * intensity * lightBrightness;
        } else {
            totalLight += BlinnPhong(L, N, V, attenuation, lightColor, intensity);
        }
    }

    // ---------- 6. Ausgabe ----------
    outNormal = vec4(N * 0.5 + 0.5, 1.0);
    outColor  = albedo;
    outLight  = vec4(totalLight, 1.0);
    outID     = pc.objID;
}
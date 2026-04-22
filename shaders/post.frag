#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outFinal;

layout(set = 2, binding = 0) uniform sampler2D tNormal;
layout(set = 2, binding = 1) uniform sampler2D tColor;
layout(set = 2, binding = 2) uniform sampler2D tLight;
layout(set = 2, binding = 3) uniform sampler2D tDepth;

layout(set = 3, binding = 0) uniform PostPC {
    vec4 resolution; // x, y, 1/x, 1/y
    vec4 params;     // x: normalEdgeStrength, y: depthEdgeStrength, z: posterizeSteps, w: debugMode
} pc;

float getDepth(int x, int y) {
    return texture(tDepth, vUv + vec2(x, y) * pc.resolution.zw).r;
}

vec3 getNormal(int x, int y) {
    return texture(tNormal, vUv + vec2(x, y) * pc.resolution.zw).rgb * 2.0 - 1.0;
}

float depthEdgeIndicator(float depth, vec3 normal) {
    float diff = 0.0;
    diff += clamp(depth - getDepth(1, 0), 0.0, 1.0);
    diff += clamp(depth - getDepth(-1, 0), 0.0, 1.0);
    diff += clamp(depth - getDepth(0, 1), 0.0, 1.0);
    diff += clamp(depth - getDepth(0, -1), 0.0, 1.0);
    return floor(smoothstep(0.01, 0.02, diff) * 2.0) / 2.0;
}

float neighborNormalEdgeIndicator(int x, int y, float depth, vec3 normal) {
    float depthDiff = depth - getDepth(x, y);
    vec3 neighborNormal = getNormal(x, y);
    
    // Edge pixels should yield to faces whose normals are closer to the bias normal.
    vec3 normalEdgeBias = vec3(1., 1., 1.); 
    float normalDiff = dot(normal - neighborNormal, normalEdgeBias);
    float normalIndicator = clamp(smoothstep(-.01, .01, normalDiff), 0.0, 1.0);
    
    // Only the shallower pixel should detect the normal edge.
    // In Reverse-Z, shallower = larger value, so depthDiff (depth - neighbor) is positive if I am shallower?
    // Wait: depth is current pixel. getDepth(x,y) is neighbor.
    // If I am at 0.6 (shallower) and neighbor at 0.5 (deeper).
    // depthDiff = 0.6 - 0.5 = 0.1.
    // sign(0.1 * .25 + .0025) = 1.
    // So the SHALLOWER pixel detects the edge. Correct.
    float depthIndicator = clamp(sign(depthDiff * .25 + .0025), 0.0, 1.0);

    return (1.0 - dot(normal, neighborNormal)) * depthIndicator * normalIndicator;
}

float normalEdgeIndicator(float depth, vec3 normal) {
    float indicator = 0.0;
    indicator += neighborNormalEdgeIndicator(0, -1, depth, normal);
    indicator += neighborNormalEdgeIndicator(0, 1, depth, normal);
    indicator += neighborNormalEdgeIndicator(-1, 0, depth, normal);
    indicator += neighborNormalEdgeIndicator(1, 0, depth, normal);

    // Amplify the indicator and use a more robust threshold.
    // dot(n1, n2) of 0.99 (~8 degrees) gives 0.01.
    // 0.01 * 20.0 = 0.2.
    // smoothstep(0.1, 0.2, 0.2) = 1.0.
    return smoothstep(0.1, 0.2, indicator * 20.0);
}

void main() {
    vec4 albedo = texture(tColor, vUv);
    vec3 light = texture(tLight, vUv).rgb;
    
    // Light is already posterized in the geometry pass.
    // We just ensure a minimum ambient level for visual clarity.
    light = max(light, vec3(0.1));

    vec3 texel = albedo.rgb * light;

    float depth = 0.0;
    vec3 normal = vec3(0.0);

    float nStrength = pc.params.x;
    float dStrength = pc.params.y;

    if (dStrength > 0.0 || nStrength > 0.0) {
        depth = getDepth(0, 0);
        normal = getNormal(0, 0);
    }

    float dei = 0.0;
    if (dStrength > 0.0) 
        dei = depthEdgeIndicator(depth, normal);

    float nei = 0.0; 
    if (nStrength > 0.0) 
        nei = normalEdgeIndicator(depth, normal);

    // Combine both edge types — take the stronger edge, then darken.
    // Previously: nei term was ignored when dei > 0, and nei brightened instead of darkened.
    float edgeFactor = max(dStrength * dei, nStrength * nei);
    float finalStrength = 1.0 - edgeFactor;

    // Debugging
    int mode = int(pc.params.w);
    if (mode == 1) { outFinal = vec4(normal * 0.5 + 0.5, 1.0); return; }
    if (mode == 2) { outFinal = vec4(albedo.rgb, 1.0); return; }
    if (mode == 3) { outFinal = vec4(light, 1.0); return; }
    if (mode == 4) { outFinal = vec4(vec3(depth), 1.0); return; }
    if (mode == 5) { outFinal = vec4(vec3(dei), 1.0); return; }
    if (mode == 6) { outFinal = vec4(vec3(nei), 1.0); return; }

    outFinal = vec4(texel * finalStrength, 1.0);
}


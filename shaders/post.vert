#version 450

layout(location = 0) out vec2 vUv;

void main() {
    // Standard fullscreen triangle UV generation
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    
    // Flip Y: uv.y is 0 at top, 1 at bottom in many desktop APIs
    // If the image is upside down, we invert it here.
    vUv = vec2(uv.x, 1.0 - uv.y);
    
    gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

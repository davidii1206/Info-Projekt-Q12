#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform PushConstants {
    mat4 model;
    mat4 viewProj;
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = pc.viewProj * worldPos;
}

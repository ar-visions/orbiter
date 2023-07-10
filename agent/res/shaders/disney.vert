#version 450

#define MAX_PBR_LIGHTS 3

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 light_pos [MAX_PBR_LIGHTS];
    vec4 light_rgba[MAX_PBR_LIGHTS];
} ubo;

layout(location = 0) in  vec3 in_pos;
layout(location = 1) in  vec3 in_normal;
layout(location = 2) in  vec2 in_uv;

layout(location = 0) out vec3 out_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_pos, 1.0);
    out_pos     = (ubo.model * vec4(in_pos, 1.0)).xyz;
    out_normal  = normalize((ubo.model * vec4(in_normal, 0.0)).xyz);
    out_uv      = in_uv;
}
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 light_dir;
    vec3 light_rgba;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    vec3  normal   = normalize(fragNormal);
    float diff     = max(dot(normal, -ubo.light_dir), 0.0);
    vec3  diffuse  = diff * ubo.light_rgba;
    vec4  texColor = texture(texSampler, fragTexCoord);

    outColor       = vec4(fragColor * diffuse, 1.0) * texColor;
}
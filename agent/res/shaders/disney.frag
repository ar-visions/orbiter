#version 450

#define MAX_PBR_LIGHTS 3

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 light_pos [MAX_PBR_LIGHTS];
    vec4 light_rgba[MAX_PBR_LIGHTS];
} ubo;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 outColor;

uniform sampler2D tx_color;
uniform sampler2D tx_normal;
uniform sampler2D tx_metallic_roughness;
uniform sampler2D tx_ao;

void main() {
    vec3  mr          = texture(tx_metallic_roughness, in_uv).rgb;
    vec3  color       = texture(tx_color,  in_uv).rgb;
    vec3  normal      = normalize(texture(tx_normal, in_uv).rgb * 2.0 - 1.0);
    float metallic    = texture(tx_metallic_roughness, in_uv).r;
    float roughness   = texture(tx_metallic_roughness, in_uv).g;
    float ao          = texture(tx_ao, in_uv).r;
    vec3  N           = normalize(in_normal);
    vec3  V           = normalize(-in_position);
    vec3  total_light = vec3(0.0);

    for (int i = 0; i < MAX_PBR_LIGHTS; ++i) {
        vec3  light    = ubo.light_rgba[i].rgb * ubo.light_rgba[i].a;

        vec3  L        = normalize(ubo.light_pos[i] - in_pos);
        vec3  H        = normalize(V + L);

        float NdotL    = max(dot(N, L), 0.0);
        float NdotV    = max(dot(N, V), 0.0);
        float NdotH    = max(dot(N, H), 0.0);
        float LdotH    = max(dot(L, H), 0.0);

        vec3  diffuse  = color * (1.0 - metallic);
        vec3  specular = vec3(0.04) + (color - vec3(0.04)) * pow(1.0 - NdotV, 5.0);
        vec3  f0       = mix(vec3(0.04), color, metallic);
        vec3  f90      = vec3(1.0);
        float D        = exp(-pow(roughness, 2.0) / (2.0 * roughness * roughness * NdotH + 0.001));
        float G        = min(1.0, min(2.0 * NdotH * NdotV / LdotH, 2.0 * NdotH * NdotL / LdotH));
        vec3  F        = f0 + (f90 - f0) * pow(1.0 - LdotH, 5.0);

        vec3 specularBRDF = (D * G * F) / (4.0 * NdotL * NdotV + 0.001);
        vec3 diffuseBRDF  = (1.0 - specularBRDF) * diffuse / (3.14159 + 0.001);
        
        total_light   += (diffuseBRDF + specularBRDF * specular) * light;
    }

    vec3 ambient       = color * ao;
    vec3 final_color   = ambient + total_light;
    outColor           = vec4(final_color, 1.0);
}
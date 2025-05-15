#include <earth>

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec3 v_normal;
layout(location = 1) in  vec2 v_uv;

void main() {
    vec2 uv = v_uv;

    // === Lighting setup ===
    vec3 normal   = normalize(v_normal);
    vec3 sun_dir  = normalize(vec3(0.2, 0.0, 0.8)); // fixed light source
    vec3 view_dir = normalize(vec3(0.0, 0.0, 1.0)); // camera points -Z

    float NdotL   = max(dot(normal, sun_dir), 0.0);
    vec3 half_dir = normalize(sun_dir + view_dir);
    float spec    = pow(max(dot(normal, half_dir), 0.0), 128.0);

    // === Cloud texture ===
    float cloud = texture(tx_earth_cloud, uv).x;

    // === Edge haze ===
    float edge = pow(1.0 - dot(normal, view_dir), 0.8); // fades near edge
    float haze_strength = 2.2;
    float cloud_alpha = cloud * 0.8 + edge * haze_strength;

    // === Optional haze tint ===
    vec3 base_color = vec3(1.0);
    vec3 haze_color = mix(base_color, vec3(0.6, 0.7, 1.0), edge); // soft blue rim
    vec3 final_rgb = mix(base_color, haze_color, edge);

    outColor = vec4(final_rgb, cloud_alpha);
}
 
#include <earth>

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec3 v_normal;
layout(location = 1) in  vec2 v_uv;
layout(location = 2) in  float v_cloud_avg;

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
    float cloud = texture(tx_earth_cloud, uv).x / (2.0 - v_cloud_avg);

    // === Edge haze ===
    float edge = pow(1.0 - dot(normal, view_dir), 1.2) * 4.0; // fades near edge
    float haze_strength = 1.0;
    float cloud_alpha = cloud + edge * haze_strength;

    // === Optional haze tint ===
    vec3 base_color = vec3(1.0);
    vec3 haze_color = mix(base_color, vec3(0.6, 0.8, 1.0), edge); // soft blue rim
    outColor = vec4(haze_color, cloud_alpha);
}
 
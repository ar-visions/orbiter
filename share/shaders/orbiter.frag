#include <pbr>

layout(set = 0, binding = 1) uniform Orbiter {
    vec4 pos_radius;      // xyz = center position, w = radius
    vec4 normal_falloff;  // xyz = normal vector of circle plane, w = falloff distance
    vec4 color_intensity; // rgb = color, a = intensity
    float moment;         // 0 to 1, sweep progress
    float moment_amount;  // 0 to 1, brightness boost
    float moment_angle;   // radians
} orbiter;

layout(location = 0) out vec4 fragColor;
layout(location = 1) in  vec3 v_world_normal;
layout(location = 2) in  vec3 v_world_pos;
layout(location = 3) in  vec3 v_view_pos;
layout(location = 4) in  vec2 v_uv;
layout(location = 5) in  vec3 v_color_0;
layout(location = 6) in  vec3 v_color_1;

// Normal mapping function
vec3 perturbNormal(vec3 N, vec3 V, vec2 texcoord) {
    vec3 map = texture(tx_normal, texcoord).rgb * 2.0 - 1.0;
    
    // Calculate tangent space
    vec3 q1 = dFdx(v_world_pos);
    vec3 q2 = dFdy(v_world_pos);
    vec2 st1 = dFdx(texcoord);
    vec2 st2 = dFdy(texcoord);
    
    vec3 T = normalize(q1 * st2.y - q2 * st1.y);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * map);
}

// Parallax mapping
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) {
    float height_scale = 0.04;
    float height = texture(tx_height, texCoords).r;
    vec2 p = viewDir.xy / viewDir.z * (height * height_scale);
    return texCoords - p;
}

vec3 environmentMapping(vec3 R, float roughness) {
    float lod   = roughness * 7.0; // Adjust based on your mipmap count
    float lodf  = floor(lod);
    float lodfr = lod - lodf;
    int   mip0  = int(lodf);
    int   mip1  = min(mip0 + 1, 7); 
    vec3  s0    = texture(tx_environment, vec4(R, mip0)).rgb;
    vec3  s1    = texture(tx_environment, vec4(R, mip1)).rgb;
    return mix(s0, s1, lodfr);
}


vec4 calculatePBR2(vec2 texCoords, vec3 worldPos, vec3 normal, vec3 viewPos) {
    // Camera and world parameters
    vec3  cameraPos        = world.pos.xyz;

    // View direction
    vec3  V                = normalize(cameraPos - worldPos);
    
    // Parallax and normal mapping
    vec2  texCoordsMapped  = parallaxMapping(texCoords, V);
    vec3  N                = perturbNormal(normal, V, texCoordsMapped);

    // Texture sampling
    vec4  baseColor        = texture(tx_color,    texCoordsMapped);
   
    float metallic         = texture(tx_metal,    texCoordsMapped).r;
    float roughness        = texture(tx_rough,    texCoordsMapped).r;
    float ao               = texture(tx_ao,       texCoordsMapped).r;
    vec3  emission         = texture(tx_emission, texCoordsMapped).rgb;
    float intensity        = texture(tx_emission, texCoordsMapped).a;

    float ior              = texture(tx_ior,      texCoordsMapped).r;
    

    // Calculate orbiter rotation

    // Orbiter parameters
    float orb_falloff      = orbiter.normal_falloff.w;
    vec3  orb_color        = orbiter.color_intensity.rgb;
    float orb_intensity    = orbiter.color_intensity.a;
    
    vec3 center = vec3(0.0, -0.07, 0.0); // orbiter.pos_radius.xyz;

    // Rough color preparation
    vec3  rough_color      = baseColor.rgb;
    
    // Reflection vector
    vec3  R                = reflect(-V, N);
    
    // Environment mapping
    vec3  envColor         = environmentMapping(R, roughness);
    
    // Directional light
    vec3  lightDir         = normalize(vec3(0.0, 1.0, 0.0));
    vec3  lightColor       = vec3(0.0, 0.04, 0.2);
    
    // BRDF calculation for main directional light
    
    float global_lighting  = 0.02;

    if (ior > 3.0)
        global_lighting = 0.2;//1.5 * metallic * (1.0 - roughness);

    // Orbiter light contribution
    vec3  orbiter_contrib  = (2.2 * v_color_1) * (orb_color * (1.0 - metallic)) * orb_intensity * 0.1;
    
    vec3  ambient          = envColor * global_lighting;// * global_lighting * rough_color * ao;
    
    // Final color composition
    vec3  Lo               = BRDF(lightDir, V, N, rough_color, ior, metallic, roughness) * lightColor;
    vec3  color            = ambient + Lo + emission * intensity + orbiter_contrib;
    
    // Color space conversion
    color = linearToSRGB(color);
    
    return vec4(color, baseColor.a);
}

void main() {
    // Calculate PBR lighting
    fragColor = calculatePBR2(v_uv, v_world_pos, normalize(v_world_normal), v_view_pos);
    //fragColor = vec4(vec3(texture(tx_ior, v_uv).x / 4.5), 1.0);
}

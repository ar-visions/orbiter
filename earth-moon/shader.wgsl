struct CameraUniform {
    view_proj: mat4x4<f32>,
    camera_pos: vec4<f32>,
}
@group(0) @binding(0) var<uniform> camera: CameraUniform;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
}

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_pos: vec3<f32>,
    @location(1) normal: vec3<f32>,
}

// Simplex 3D noise
fn random3(p: vec3<f32>) -> f32 {
    return fract(sin(dot(p, vec3<f32>(127.1, 311.7, 74.7))) * 43758.5453123);
}

fn noise3d(p: vec3<f32>) -> f32 {
    let i = floor(p);
    let f = fract(p);
    
    // Cubic interpolation
    let u = f * f * (3.0 - 2.0 * f);
    
    let h1 = mix(
        mix(
            mix(random3(i), 
                random3(i + vec3<f32>(1.0, 0.0, 0.0)), u.x),
            mix(random3(i + vec3<f32>(0.0, 1.0, 0.0)),
                random3(i + vec3<f32>(1.0, 1.0, 0.0)), u.x),
            u.y),
        mix(
            mix(random3(i + vec3<f32>(0.0, 0.0, 1.0)),
                random3(i + vec3<f32>(1.0, 0.0, 1.0)), u.x),
            mix(random3(i + vec3<f32>(0.0, 1.0, 1.0)),
                random3(i + vec3<f32>(1.0, 1.0, 1.0)), u.x),
            u.y),
        u.z
    );
    
    return h1;
}

@vertex
fn vs_main(model: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.clip_position = camera.view_proj * vec4<f32>(model.position, 1.0);
    out.world_pos = model.position;
    out.normal = model.normal;
    return out;
}

fn ray_sphere_intersect(ray_origin: vec3<f32>, ray_dir: vec3<f32>, radius: f32) -> vec2<f32> {
    let a = dot(ray_dir, ray_dir);
    let b = 2.0 * dot(ray_dir, ray_origin);
    let c = dot(ray_origin, ray_origin) - radius * radius;
    let discriminant = b * b - 4.0 * a * c;
    
    if discriminant < 0.0 {
        return vec2<f32>(-1.0, -1.0);
    }
    
    let t1 = (-b - sqrt(discriminant)) / (2.0 * a);
    let t2 = (-b + sqrt(discriminant)) / (2.0 * a);
    return vec2<f32>(t1, t2);
}

fn get_surface_color(pos: vec3<f32>, normal: vec3<f32>) -> vec3<f32> {
    let light_dir = normalize(vec3<f32>(1.0, 1.0, 1.0));
    let base_color = vec3<f32>(0.2, 0.4, 0.8);
    let ambient = 0.0;
    let diffuse = max(dot(normal, light_dir), 0.0);
    return base_color * (ambient + diffuse);
}

fn sample_volume(pos: vec3<f32>) -> vec4<f32> {
    let dist_from_center = length(pos);
    let normalized_height = (dist_from_center - 0.95) / 0.05;

    // Calculate view angle factor for maximum haze near the horizon
    let view_dir = normalize(pos - camera.camera_pos.xyz);
    let up_dir = normalize(pos);
    let view_angle = abs(dot(view_dir, up_dir));
    let horizon_density = pow(1.0 - view_angle, 4.0); // Sharpen density near the edge

    // Exponential density falloff for atmosphere
    let base_density = exp(-normalized_height * 3.0);
    let max_density = 5.0; // High density to completely obscure edges
    let density = clamp(base_density + horizon_density * max_density, 0.0, 1.0);

    // Atmospheric color blending
    let atmo_color = vec3<f32>(0.8, 0.9, 1.0); // Bright haze color
    let color = atmo_color * density; // Ensure haze dominates near the edge

    return vec4<f32>(color, density);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let ray_origin = camera.camera_pos.xyz;
    let ray_dir = normalize(in.world_pos - ray_origin);
    
    let solid_hit = ray_sphere_intersect(ray_origin, ray_dir, 0.95);
    
    var final_color = vec3<f32>(0.0);
    var final_alpha = 0.0;
    
    if (solid_hit.x > 0.0) {
        let hit_point = ray_origin + ray_dir * solid_hit.x;
        let normal = normalize(hit_point);
        final_color = get_surface_color(hit_point, normal);
        final_alpha = 1.0;
    }
    
    let outer_hit = ray_sphere_intersect(ray_origin, ray_dir, 1.0);
    if (outer_hit.x > 0.0) {
        let start_t = max(outer_hit.x, select(outer_hit.x, solid_hit.y, solid_hit.x > 0.0));
        let end_t = outer_hit.y;
        
        let steps = 32u;
        let step_size = (end_t - start_t) / f32(steps);
        
        var accumulated_color = vec3<f32>(0.0);
        var accumulated_alpha = 0.0;
        
        if (start_t < end_t) {
            for (var i = 0u; i < steps && accumulated_alpha < 0.99; i = i + 1u) {
                let t = start_t + step_size * f32(i);
                let pos = ray_origin + ray_dir * t;
                let sample = sample_volume(pos);
                
                let alpha = sample.a * step_size * 2.0;
                accumulated_color += (1.0 - accumulated_alpha) * sample.rgb * alpha;
                accumulated_alpha += (1.0 - accumulated_alpha) * alpha;
            }
        }
        
        final_color = mix(final_color, accumulated_color, accumulated_alpha);
        final_alpha = max(final_alpha, accumulated_alpha);
    }
    
    return vec4<f32>(final_color, final_alpha);
}

#include <import>

typedef struct Uniforms {
    World world;
    Rays  rays;
} Uniforms;

void orbiter_frame(pipeline p, Uniforms* u) {
    vec3f   eye         = vec3f   (0.0f,  3.5f, -2.0f);
    vec3f   target      = vec3f   (0.0f,  0.0f,  0.0f);
    vec3f   up          = vec3f   (0.0f, -1.0f,  0.0f);

    static float sequence = 0;
    sequence += 0.01f;
    vec4f v = vec4f(0.0f, 1.0f, 0.0f, radians(sequence));
    quatf q = quatf(&v);
    u->world->pos   = vec4f(&eye);
    u->world->model = mat4f_rotate     (&u->world->model, &q);
    u->world->proj  = mat4f_perspective(radians(40.0f), 1.0f, 0.1f, 100.0f);
    u->world->view  = mat4f_look_at    (&eye, &target, &up);
}

void opencv_blur_equirect(uint8_t* input, int w, int h);

int main(int argc, cstr argv[]) {
    A_start();
    Uniforms u = {};

    path    gltf        = path    ("models/orbiter.gltf");
    path    gltf_march  = path    ("models/ray-march.gltf"); // needs a model matrix to 
    Model   orbiter     = read    (gltf,       typeid(Model));
    Model   march       = read    (gltf_march, typeid(Model));
    trinity t           = trinity ();
    window  w           = window  (t, t, title, string("orbiter"), width, 800, height, 800 );
    shader  pbr         = shader  (t, t, name,  string("pbr"));
    shader  shader_rays = shader  (t, t, name,  string("ray-march"));
    image   environment = image   (uri, form(path, "images/forest.exr"), surface, Surface_environment);

    u.world = World();
    u.rays  = Rays();
    model   agent       = model   (t, t, w, w, id, orbiter, shader, pbr,
                                        samplers, a(environment, null),
                                        uniforms, a(u.world, null));
    model   ray_march   = model   (t, t, w, w, id, march, shader, shader_rays,
                                        uniforms, a(u.rays, null));
    push(w, agent);
    push(w, ray_march);
    return loop(w, orbiter_frame, &u);
}

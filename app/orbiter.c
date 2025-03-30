
#include <import>

void orbiter_frame(pipeline p, World world) {
    vec3f   eye         = vec3f   (0.0f,  3.5f, -2.0f);
    vec3f   target      = vec3f   (0.0f,  0.0f,  0.0f);
    vec3f   up          = vec3f   (0.0f, -1.0f,  0.0f);

    static float sequence = 0;
    sequence += 0.01f;
    vec4f v = vec4f(0.0f, 1.0f, 0.0f, radians(sequence));
    quatf q = quatf(&v);
    world->pos   = vec4f(&eye);
    world->model = mat4f_rotate     (&world->model, &q);
    world->proj  = mat4f_perspective(radians(40.0f), 1.0f, 0.1f, 100.0f);
    world->view  = mat4f_look_at    (&eye, &target, &up);
}

void opencv_blur_equirect(uint8_t* input, int w, int h);

int main(int argc, cstr argv[]) {
    A_start();

    path    gltf        = path    ("models/sphere.gltf" );
    Model   orbiter     = read    (gltf, typeid(Model) );
    trinity t           = trinity ();
    window  w           = window  (t, t, title, string("orbiter"), width, 800, height, 800 );
    shader  pbr         = shader  (t, t, name,  string("pbr"));
    image   environment = image   (uri, form(path, "images/forest.exr"), surface, Surface_environment);

    //opencv_blur_equirect((uint8_t*)data(environment), environment->width, environment->height);
    
    World   world       = World   ();
    model   agent       = model   (t, t, w, w, id, orbiter, shader, pbr,
                                        samplers, a(environment, null),
                                        uniforms, a(world, null));
    push(w, agent);
    return loop(w, orbiter_frame, world);
}
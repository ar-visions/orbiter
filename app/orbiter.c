
#include <import>
#include <trinity>
#include <math.h>

typedef struct Uniforms {
    Orbiter orbiter;
    UVQuad  canvas;
} Uniforms;

mat4f blast_matrix() {
    mat4f m;
    for (int i = 0; i < 16; i++)
        m.m[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    return m;
}

void orbiter_frame(pipeline p, Uniforms* u) {
    vec3f   eye         = vec3f   (0.0f,  3.5f, -2.0f);
    vec3f   target      = vec3f   (0.0f,  0.0f,  0.0f);
    vec3f   up          = vec3f   (0.0f, -1.0f,  0.0f);

    static float sequence = 0;
    sequence += 0.0022f;

    static float m_sequence = 0;
    m_sequence += 0.00022f;

    vec4f v              = vec4f(0.0f, 1.0f, 0.0f, radians(sequence));
    quatf q              = quatf(&v);
    Orbiter orb          = u->orbiter;
    orb->pos             = vec4f(&eye);
    orb->model           = mat4f_rotate     (&u->orbiter->model, &q);
    orb->proj            = mat4f_perspective(radians(40.0f), 1.0f, 0.1f, 100.0f);
    orb->view            = mat4f_look_at    (&eye, &target, &up);
    orb->moment          = m_sequence;
    orb->moment_angle    = 0.5f;
    orb->moment_amount   = 10.0f;
    orb->pos_radius      = vec4f(0.92, 0.0, -0.09, 0.5f);
    orb->normal_falloff  = vec4f(0.0,  1.0, 0.0, 0.8f);
    orb->color_intensity = vec4f(0.6,  0.3, 1.0, 4.0f);

    if (orb->moment_angle > M_PI) orb->moment_angle = orb->moment_angle - M_PI;
    
    u->canvas->model = mat4f_ident();



    // rotate 90 deg X
    vec4f rotation_axis_angle = vec4f(1.0f, 0.0f, 0.0f, radians(90.0f));
    quatf rotation_quat = quatf(&rotation_axis_angle);
    u->canvas->model = mat4f_rotate(&u->canvas->model, &rotation_quat);

    //vec3f sc1 = vec3f(0.1f, 100.0f, 1000.0f);
    //u->canvas->model = mat4f_scale(&u->canvas->model, &sc1);
    
    /// move down
    vec3f tr = vec3f(0.0f, 0.5f, 0.0f);
    u->canvas->model = mat4f_translate(&u->canvas->model, &tr);

    u->canvas->view  = mat4f_ident();
    u->canvas->proj  = mat4f_ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
}

int main(int argc, cstr argv[]) {
    A_start();
    path_app(argv[0]);
    trinity t = trinity();
 
    //isolate(t);
    Uniforms u = {};
    path    gltf        = path    ("models/orbiter2.gltf");
    path    gltf_quad   = path    ("models/uv-quad.gltf"); // needs a model matrix to 
    Model   orbiter     = read    (gltf, typeid(Model));
    Model   quad        = read    (gltf_quad, typeid(Model));
    window  w           = window  (t, t, title, string("orbiter"), width, 800, height, 800 );
    image   environment = image   (uri, form(path, "images/forest.exr"), surface, Surface_environment);

    u.orbiter = Orbiter(t, t, name, string("orbiter"));
    u.canvas  = UVQuad (t, t, name, string("uv-quad"));

    canvas  cv = canvas(t, t, format, Pixel_rgba8, surface, Surface_color, width, 800, height, 800);
    clear(cv, string("#ffff"));
    sync(cv);
    
    model   agent   = model(t, t, w, w, id, orbiter, s, u.orbiter, samplers, a(environment, null));
    model   mcanvas = model(t, t, w, w, id, quad,    s, u.canvas,  samplers, a(cv,          null));
    push(w, agent);
    push(w, mcanvas);

    
    
    return loop(w, orbiter_frame, &u);
}


define_mod(UVQuad, shader)

void Orbiter_init(Orbiter w) {
    w->pos_radius      = vec4f(0.0, 0.1, 0.0, 0.3f);
    w->normal_falloff  = vec4f(0.0, 1.0, 0.0, 1.0f);
    w->color_intensity = vec4f(0.6, 0.3, 1.0, 4.0f);
    w->moment          = 0.0;
    w->moment_amount   = 1.0f;
    w->moment_angle    = 0.0f;
}

define_mod(Orbiter, PBR)

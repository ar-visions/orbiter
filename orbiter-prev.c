
#include <import>
#include <trinity>
#include <math.h>


static window main_window;

typedef struct Shaders {
    window  w;
    Orbiter orbiter;
    Audrey  au;
    UVQuad  screen;
    BlurV   blur_v;
    Blur    blur;
    canvas  beam;
    canvas  core;
    canvas  canvas;
} Shaders;

mat4f blast_matrix() {
    mat4f m;
    for (int i = 0; i < 16; i++)
        m.m[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    return m;
}

void orbiter_mousemove() {
}

void orbiter_frame(Shaders* u) {
    window w = u->w;
    vec3f   eye         = vec3f   (0.0f,  8.0f + w->debug_value,  4.0f);
    vec3f   target      = vec3f   (0.0f,  0.0f,  0.0f);
    vec3f   up          = vec3f   (0.0f, -1.0f,  0.0f);

    static float sequence = 0;
    sequence += 0.0022f;
    static float m_sequence = 0;
    m_sequence += 0.00022f;

    vec4f v              = vec4f(0.0f, 1.0f, 0.0f, radians(0.0));
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

    u->au->model = mat4f_ident();
    // rotate 90 deg X
    vec4f rotation_axis_angle = vec4f(1.0f, 0.0f, 0.0f, radians(90.0f));
    quatf rotation_quat       = quatf(&rotation_axis_angle);
    u->au->model = mat4f_rotate(&u->au->model, &rotation_quat);
    //vec3f sc1 = vec3f(0.1f, 100.0f, 1000.0f);
    //u->canvas->model = mat4f_scale(&u->canvas->model, &sc1);
    /// move down
    vec3f tr              = vec3f(0.0f, 0.5f, 0.0f);
    u->au->model          = mat4f_translate(&u->au->model, &tr);
    u->au->view           = mat4f_ident();
    u->au->proj           = mat4f_ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    u->au->v_model        = orb->model;
    u->au->v_view         = orb->view;
    u->au->v_proj         = orb->proj;
    u->au->time           = sequence;
    u->au->thrust         = 1.00;
    u->au->magnitude      = 1.00;
    u->au->colors         = 0.22;
    u->au->halo_attenuate = 0.44;
    u->au->halo_space     = 0.44;
    u->au->halo_separate  = 0.11;
    u->au->halo_thickness = 0.22;
}

int main(int argc, cstr argv[]) {
    A_start();
    path_app(argv[0]);
    trinity t      = trinity();
    int     width  = 800,
            height = 800;
 
    Shaders u = {
        .w = window (
            t, t, title, string("orbiter"),
            width, width, height, height )
    };
    path    gltf      = path    ("models/orbiter2.gltf");
    path    gltf_quad = path    ("models/uv-quad.gltf");
    
    Model   orbiter   = read    (gltf,      typeid(Model));
    Model   quad      = read    (gltf_quad, typeid(Model));
    image   env       = image   (
        uri, form(path, "images/forest.exr"),
        surface, Surface_environment);

    vec4f rotation_axis_angle = vec4f(1.0f, 0.0f, 0.0f, radians(90.0f));
    quatf rotation_quat       = quatf(&rotation_axis_angle);
    vec3f tr                  = vec3f(0.0f, 0.5f, 0.0f);

    u.orbiter = Orbiter (t, t, name, string("orbiter"));
    u.au      = Audrey  (t, t, name, string("audrey"));
    u.blur_v  = BlurV   (t, t, name, string("blur-v"));
    u.blur    = Blur    (t, t, name, string("blur"));
    u.screen  = UVQuad  (t, t, name, string("uv-quad"));
    
    /// set model matrix
    u.screen->model = mat4f_ident    ();
    u.screen->model = mat4f_rotate   (&u.screen->model, &rotation_quat);
    u.screen->model = mat4f_translate(&u.screen->model, &tr);

    u.screen->view  = mat4f_ident();
    u.screen->proj  = mat4f_ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    u.canvas        = canvas  (
        t, t, format, Pixel_rgba8, surface, Surface_color,
        width, width, height, height);
    clear     (u.canvas, string("#0000"));
    rect      (u.canvas, 0, 0, 100, 100);
    fill_color(u.canvas, string("#fff"));
    draw_fill (u.canvas, false);
    sync      (u.canvas);


    canvas     core = canvas  (
        t, t, format, Pixel_rgba8, surface, Surface_emission,
        width, width, height, height);
    clear     (core, string("#0000"));
    
    /// outter color
    rect      (core, 0, 0, 800, 800);
    fill_color(core, string("#f00"));
    draw_fill (core, false);

    /// inner core
    arc       (core, 400.0f, 400.0f, 100.0f, 0.0f, 360.0f);
    fill_color(core, string("#a0f"));
    draw_fill (core, false);

    /// core mantle!
    arc       (core, 400.0f, 400.0f, 40.0f, 0.0f, 360.0f);
    fill_color(core, string("#aff"));
    draw_fill (core, false);
    sync      (core);
    u.core   = core;

    canvas     beam = canvas  (
        t, t, format, Pixel_rgba8, surface, Surface_color,
        width, width, height, height);
    clear     (beam, string("#0000"));
    rect      (beam, 0, 0, 800, 800);
    fill_color(beam, string("#00f"));
    draw_fill (beam, false);
    arc       (beam, 400.0f, 400.0f, 40.0f, 0.0f, 360.0f);
    fill_color(beam, string("#aff"));
    draw_fill (beam, false);
    sync      (beam);
    u.beam   = beam;

    /// create agent model with texture associated to render
    window  w         = u.w;
    model   m_agent   = model  (w, w, id, orbiter, s, u.orbiter, samplers, a(env));
    model   m_au      = model  (w, w, s, u.au, samplers, a(u.core, u.beam));
    render  r_agent   = render (w, w,
        clear_color, vec4f(0.0, 0.5, 0.5, 1.0), 
        models,      a(m_agent, m_au));
    
    model   m_blur_v  = model  (w, w, s, u.blur_v, samplers, a(r_agent, m_au));
    render  r_blur_v  = render (w, w,
        clear_color, vec4f(1.0, 0.0, 1.0, 1.0), models, a(m_blur_v));

    model   m_blur    = model  (w, w, s, u.blur,   samplers, a(r_blur_v));
    render  r_blur    = render (w, w,
        clear_color, vec4f(1.0, 1.0, 0.0, 1.0), models, a(m_blur));
    
    model   m_screen  = model  (w, w, s, u.screen, samplers, a(r_blur));
    render  r_screen  = render (w, w,
        clear_color, vec4f(0.0, 1.0, 0.0, 1.0), models, a(m_screen));

    w->list = a(r_agent, r_blur_v, r_blur, r_screen);

    return loop(w, orbiter_frame, &u);
}

define_mod(Audrey, shader)
define_mod(BlurV,  shader)
define_mod(Blur,   shader)

void Orbiter_init(Orbiter w) {
    w->pos_radius      = vec4f(0.0, 0.1, 0.0, 0.3f);
    w->normal_falloff  = vec4f(0.0, 1.0, 0.0, 1.0f);
    w->color_intensity = vec4f(0.6, 0.3, 1.0, 4.0f);
    w->moment          = 0.0;
    w->moment_amount   = 1.0f;
    w->moment_angle    = 0.0f;
}

define_mod(Orbiter, PBR)

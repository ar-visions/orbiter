
#include <import>
#include <math.h>

static window main_window;

typedef struct Shaders {
    window  w;
    Orbiter orbiter;
    Audrey  au;
    UVQuad  screen;
    BlurV   blur_v;
    Blur    blur;
    sk      beam;
    sk      core;
    sk      cv;
} Shaders;

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
    //u->cv->model = mat4f_scale(&u->cv->model, &sc1);
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

    sk cv = u->cv;
    clear     (cv, string("#111"));
    static float x = 0;
    static float y = 0;

    if (cv->width != u->w->width || cv->height != u->w->height) {
        resize_texture(cv, u->w->width, u->w->height);
    }

    x += 1;
    if (x > 800) {
        y += 1;
        x = 0.0;
    }
    
    rect_to   (cv, x, y, 400, 400);
    fill_color(cv, string("#0af"));
    draw_fill (cv, false);
    sync      (cv);
    output_mode(cv, true);
}


int main(int argc, cstr argv[]) {
    A_start();
    path_app(argv[0]);
    trinity t      = trinity();
    int     width  = 800, height = 800;
    Shaders u = { 
        .w = window(
                t, t, title, string("orbiter-canvas"),
                width, width, height, height ),
        .cv = sk(
            t, t, format, Pixel_rgba8, surface, Surface_color,
            width, width, height, height)
    };

    window  w         = u.w;
    Model   orbiter   = read (f(path, "models/orbiter2.gltf"), typeid(Model));
    image   env       = image(
        uri, f(path, "images/forest.exr"),
        surface, Surface_environment);

    u.orbiter = Orbiter (t, t, name, string("orbiter"));
    u.au      = Audrey  (t, t, name, string("audrey"));
    u.blur    = Blur    (t, t, name, string("blur"));
    
    u.cv      = sk(
        t, t, format, Pixel_rgba8, surface, Surface_color,
        width, width, height, height);
    
    sk core = sk(
        t, t, format, Pixel_rgba8, surface, Surface_emission,
        width, width, height, height);
    clear     (core, string("#0000"));
    
    /// outter color
    rect_to   (core, 0, 0, 800, 800);
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

    sk beam = sk(
        t, t, format, Pixel_rgba8, surface, Surface_color,
        width, width, height, height);
    clear     (beam, string("#0000"));
    rect_to   (beam, 0, 0, 800, 800);
    fill_color(beam, string("#00f"));
    draw_fill (beam, false);
    arc       (beam, 400.0f, 400.0f, 40.0f, 0.0f, 360.0f);
    fill_color(beam, string("#aff"));
    draw_fill (beam, false);
    sync      (beam);
    u.beam   = beam;

    model   m_agent   = model  (w, w, id, orbiter, s, u.orbiter, samplers, a(env));
    model   m_au      = model  (w, w, s, u.au, samplers, a(u.core->tx, u.beam->tx));
    render  r_agent   = render (w, w, clear_color, vec4f(0.0, 0.5, 0.5, 1.0), models, a(m_agent, m_au));
    /*
    model   m_blur_v  = model  (w, w, s, BlurV(t, t, name, string("blur-v")), samplers, a(r_agent->color));
    render  r_blur_v  = render (w, w, clear_color, vec4f(0.0, 0.0, 1.0, 1.0), models, a(m_blur_v));

    model   m_blur    = model  (w, w, s, Blur(t, t, name, string("blur")), samplers, a(r_blur_v->color));
    render  r_blur    = render (w, w, clear_color, vec4f(0.5, 0.0, 0.0, 1.0), models, a(m_blur));
    
    model   m_view    = model  (w, w, s, UXQuad(t, t, name, string("ux")), samplers, a(r_blur->color, u.cv->tx));
    */

    model   m_view    = model  (w, w, s, UXQuad(t, t, name, string("ux")), samplers, a(r_agent->color, u.cv->tx));
    render  r_view    = render (w, w, clear_color, vec4f(0.0, 0.5, 0.5, 1.0), models, a(m_view));

    //w->list = a(r_agent, r_blur_v, r_blur, r_view);
    w->list = a(r_agent, r_view);
    return loop(w, orbiter_frame, &u);
}

define_mod(Audrey, shader)

void Orbiter_init(Orbiter w) {
    w->pos_radius      = vec4f(0.0, 0.1, 0.0, 0.3f);
    w->normal_falloff  = vec4f(0.0, 1.0, 0.0, 1.0f);
    w->color_intensity = vec4f(0.6, 0.3, 1.0, 4.0f);
    w->moment          = 0.0;
    w->moment_amount   = 1.0f;
    w->moment_angle    = 0.0f;
}
 
define_mod(Orbiter, PBR)

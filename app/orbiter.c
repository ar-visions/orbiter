
#include <import>
#include <math.h>

static window main_window;

typedef struct Shaders {
    window  w;
    Earth   earth;
    Ocean   ocean; /// ocean is a separate render pass with the same model
    Cloud   cloud;
    Orbiter orbiter;
    Audrey  au;
    UVQuad  screen;
    BlurV   blur_v;
    Blur    blur;
    sk      beam;
    sk      core;
    sk      cv;
    f32     time;
} Shaders;

void orbiter_mousemove() {
}


mat4f blast_matrix() {
    mat4f m;
    for (int i = 0; i < 16; i++)
        m.m[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    return m;
}

 
void orbiter_frame(Shaders* u) {
    u->time -= 0.044f - (u->w->debug_value * 0.2);

    // step 1: apply Earth's axial tilt
    float tilt_deg = -23.44f;
    float tilt_rad = radians(tilt_deg);
    vec4f tilt_axis = vec4f(1.0f, 0.0f, 0.0f, tilt_rad); // tilt around X axis
    quatf q_tilt = quatf(&tilt_axis);
    mat4f m_tilt = mat4f(&q_tilt);

    // step 2: spin Earth around Y axis *in its local (tilted) space*
    vec4f spin_axis = vec4f(0.0f, 1.0f, 0.0f, radians(u->time));
    quatf q_spin = quatf(&spin_axis);
    mat4f m_spin = mat4f(&q_spin);

    // final model matrix: spin after tilt
    u->earth->model = mat4f_mul(&m_tilt, &m_spin);
    u->ocean->model = u->earth->model;
    u->cloud->model = u->earth->model;

    u->earth->time = u->time;
    u->ocean->time = u->time;
    u->cloud->time = u->time;
    /*
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

    //u->earth->model = blast_matrix();
    */
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
    Model   earth     = read (f(path, "models/earth.gltf"), typeid(Model));
    Model   orbiter   = read (f(path, "models/orbiter2.gltf"),   typeid(Model));
    //image   env       = image(
    //    uri, f(path, "images/forest.exr"),
    //    surface, Surface_environment);

    image  earth_color      = image(uri, f(path, "textures/earth-color-8192x4096.png"),      surface, (Surface)EarthSurface_color);
    image  earth_normal     = image(uri, f(path, "textures/earth-normal-8192x4096.png"),     surface, (Surface)EarthSurface_normal);
    image  earth_elevation  = image(uri, f(path, "textures/earth-elevation-8192x4096.png"),  surface, (Surface)EarthSurface_elevation);
    image  earth_water = image(uri, f(path, "textures/earth-water-8192x4096.png"),      surface, (Surface)EarthSurface_water);
    image  earth_water_blur = image(uri, f(path, "textures/earth-water-blur-8192x4096.png"), surface, (Surface)EarthSurface_water_blur);
    image  earth_cloud      = image(uri, f(path, "textures/earth-cloud-8192x4096.png"),      surface, (Surface)EarthSurface_cloud);
    image  earth_bathymetry = image(uri, f(path, "textures/earth-bathymetry-8192x4096.png"), surface, (Surface)EarthSurface_bathymetry);
    image  earth_lights     = image(uri, f(path, "textures/earth-lights-8192x4096.png"),     surface, (Surface)EarthSurface_lights);

    u.earth   = Earth   (t, t, name, string("earth"));
    u.ocean   = Ocean   (t, t, name, string("ocean"));
    u.cloud   = Cloud   (t, t, name, string("cloud"));

    //u.orbiter = Orbiter (t, t, name, string("orbiter"));
    //u.au      = Audrey  (t, t, name, string("audrey"));
    //u.blur    = Blur    (t, t, name, string("blur"));
    
    /*
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
    */

    model   m_earth   = model  (w, w, id, earth,   s, u.earth,
        samplers, a(
            earth_color, earth_normal, earth_elevation, earth_water, earth_water_blur, earth_cloud,
            earth_bathymetry, earth_lights));
    
    model   m_ocean   = model  (w, w, id, earth,   s, u.ocean,
        samplers, a(
            earth_color, earth_normal, earth_elevation, earth_water, earth_water_blur, earth_cloud,
            earth_bathymetry, earth_lights));

    model   m_cloud   = model  (w, w, id, earth,   s, u.cloud,
        samplers, a(
            earth_color, earth_normal, earth_elevation, earth_water, earth_water_blur, earth_cloud,
            earth_bathymetry, earth_lights));
    
    //model   m_agent   = model  (w, w, id, orbiter, s, u.orbiter, samplers, a(env));
    //model   m_au      = model  (w, w, s, u.au, samplers, a(u.core->tx, u.beam->tx));
    
    render  r_agent   = render (w, w,
        width,          w->width  * 2,
        height,         w->height * 2,
        clear_color,    vec4f(0.0, 0.1, 0.2, 1.0),
        models,         a(m_earth, m_ocean, m_cloud));
    
    //BlurV bv = BlurV(t, t, name, string("blur-v"));
    //model   m_blur_v  = model  (w, w, s, bv, samplers, a(r_agent->color));
    //render  r_blur_v  = render (w, w, clear_color, vec4f(1.0, 1.0, 1.0, 1.0), models, a(m_blur_v));

    //Blur bl = Blur(t, t, name, string("blur"));
    //model   m_blur    = model  (w, w, s, bl, samplers, a(r_blur_v->color));
    //render  r_blur    = render (w, w, clear_color, vec4f(1.0, 1.0, 1.0, 1.0), models, a(m_blur));
    
    //model   m_view    = model  (w, w, s, UXQuad(t, t, name, string("ux")), samplers, a(u.cv->tx, r_blur->color));
    //render  r_view    = render (w, w, clear_color, vec4f(1.0, 1.0, 1.0, 1.0), models, a(m_view));

    //w->list = a(r_agent, r_blur_v, r_blur, r_view);
    w->list = a(r_agent);
    return loop(w, orbiter_frame, &u);
}

define_mod(Audrey, shader)

/// drawing ortho Earth first
void Earth_init(Earth w) {
    f32   fov_deg = 60.0f;
    f32   aspect  = 1920.0f / 1080.0f;
    f32   near    = 0.1f;
    f32   far     = 100.0f;

    mat4f proj    = mat4f_perspective(radians(fov_deg), aspect, near, far);
    vec3f eye2    = vec3f   (0.0f,  0.0f,  1.5f);
    vec3f target2 = vec3f   (0.0f,  0.0f,  0.0f);
    vec3f up2     = vec3f   (0.0f, -1.0f,  0.0f);
    w->model      = mat4f_ident      ();
    w->proj       = mat4f_perspective(radians(70.0f), 1.0f, 0.1f, 100.0f);
    w->view       = mat4f_look_at    (&eye2, &target2, &up2);

}



void Orbiter_init(Orbiter w) {
    w->pos_radius      = vec4f(0.0, 0.1, 0.0, 0.3f);
    w->normal_falloff  = vec4f(0.0, 1.0, 0.0, 1.0f);
    w->color_intensity = vec4f(0.6, 0.3, 1.0, 4.0f);
    w->moment          = 0.0;
    w->moment_amount   = 1.0f;
    w->moment_angle    = 0.0f;
}
 

define_mod(Earth, shader)
define_mod(Ocean, Earth)
define_mod(Cloud, Earth)

define_mod(Orbiter, PBR)

define_enum(EarthSurface)

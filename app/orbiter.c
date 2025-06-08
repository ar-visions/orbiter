
#include <import>
#include <math.h>

static window main_window;

typedef struct Shaders {
    window  w;
    Earth   earth;
    Purple  purple;
    Ocean   ocean; /// ocean is a separate render pass with the same model
    Cloud   cloud;
    Orbiter orbiter;
    Audrey  au;
    sk      beam;
    sk      core;
    sk      cv;
    f32     time;
} Shaders;

void orbiter_mousemove() {
}

map orbiter_interface(Shaders* u) {
    pane ctrl = pane();
    map m = map_of(
        "main", pane(elements, map_of(
            "outline", element(),
            "text",    element(),
        null)),
        null);
    return m;
}
 
object orbiter_background(Shaders* u) {
    u->time += 0.0044f - (u->w->debug_value * 0.04);

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

    u->purple->model = mat4f_mul(&m_tilt, &m_spin);
    u->purple->time = u->time;

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
    return null;
}

object on_process_exit(dbg debug) {
    print("debug exit");
    return null;
}

object on_crash(cursor cur) {
    print("crash at %o:%i:%i", cur->source, cur->line, cur->column);
    return null;
}

object on_break(cursor cur) {
    print("breakpoint at %o:%i:%i", cur->source, cur->line, cur->column);
    return null;
}

object on_stdout(iobuffer b) {
    fwrite(b->bytes, 1, b->count, stdout);
    return null;
}

object on_stderr(iobuffer b) {
    fwrite(b->bytes, 1, b->count, stderr);
    return null;
}

int main(int argc, cstr argv[]) {
    A_start(argc, argv);

    path location = f(path, "/src/A/debug/test/a-test");
    path src      = f(path, "/src/A/test/a-test.c");
    printf("...\n");
    dbg debugf = calloc(1, sizeof(dbg));
    int offset_f = (i8*)&debugf->f - (i8*)debugf;
    dbg  debug    = dbg(
        location,  location,
        on_stdout, on_stdout,
        on_stderr, on_stderr,
        on_break,  on_break,
        on_crash,  on_crash,
        on_exit,   on_process_exit);
    set_breakpoint(debug, src, 19, 0);
    set_breakpoint(debug, src, 20, 0);
    start(debug);

    for (;;) {
        usleep(10000);
    }

    auto_free();
    
    trinity t      = hold(trinity());
    int     width  = 1200, height = 1200;

    Shaders u = {};
    u.w = hold(window(
        t, t, title, string("orbiter-canvas"),
        width, width, height, height ));
    
    auto_free();
    
    u.cv = hold(sk(
        t, t, format, Pixel_rgba8, 
        width, width, height, height));
    print("canvas: %o", u.cv);
    
    window  w         = u.w;
    Model   earth     = hold(read (f(path, "models/earth.gltf"), typeid(Model)));
    Model   purple    = earth;
    Model   orbiter   = hold(read (f(path, "models/orbiter2.gltf"),   typeid(Model)));
    //image   env       = image(
    //    uri, f(path, "images/forest.exr"));
    
    auto_free();

    // best to define the general ux path first
    image  earth_color      = hold(image(uri, f(path, "textures/earth-color-8192x4096.png")));
    image  earth_normal     = hold(image(uri, f(path, "textures/earth-normal-8192x4096.png")));
    image  earth_elevation  = hold(image(uri, f(path, "textures/earth-elevation-8192x4096.png")));
    image  earth_water      = hold(image(uri, f(path, "textures/earth-water-8192x4096.png")));
    image  earth_water_blur = hold(image(uri, f(path, "textures/earth-water-blur-8192x4096.png")));
    image  earth_cloud      = hold(image(uri, f(path, "textures/earth-cloud-8192x4096.png")));
    image  earth_bathymetry = hold(image(uri, f(path, "textures/earth-bathymetry-8192x4096.png")));
    image  earth_lights     = hold(image(uri, f(path, "textures/earth-lights-8192x4096.png")));
    image  purple_color     = hold(image(uri, f(path, "textures/purple-color-8192x4096.png")));
    image  purple_cloud     = hold(image(uri, f(path, "textures/purple-cloud-8192x4096.png")));

    auto_free();

    u.purple  = hold(Purple  (t, t, name, string("purple")));
    u.earth   = hold(Earth   (t, t, name, string("earth")));
    u.ocean   = hold(Ocean   (t, t, name, string("ocean")));
    u.cloud   = hold(Cloud   (t, t, name, string("cloud")));

    auto_free();

    //u.orbiter = Orbiter (t, t, name, string("orbiter"));
    //u.au      = Audrey  (t, t, name, string("audrey"));
    //u.blur    = Blur    (t, t, name, string("blur"));
    
    /*
    u.cv      = sk(
        t, t, format, Pixel_rgba8,
        width, width, height, height);
    
    sk core = sk(
        t, t, format, Pixel_rgba8,
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
        t, t, format, Pixel_rgba8, 
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
    
    model   m_purple   = hold(model  (w, w, id, purple, s, u.purple,
        samplers, map_of(
            "color",      purple_color,
            "cloud",      purple_cloud, null)));

    model   m_earth   = hold(model  (w, w, id, earth,   s, u.earth,
        samplers, map_of(
            "color",      earth_color,
            "normal",     earth_normal,
            "elevation",  earth_elevation,
            "water",      earth_water,
            "water_blur", earth_water_blur,
            "cloud",      earth_cloud,
            "bathymetry", earth_bathymetry,
            "lights",     earth_lights, null)));
    
    model   m_ocean   = hold(model  (w, w, id, earth,   s, u.ocean,
        samplers, map_of(
            "color",      earth_color,
            "normal",     earth_normal,
            "elevation",  earth_elevation,
            "water",      earth_water,
            "water_blur", earth_water_blur,
            "cloud",      earth_cloud,
            "bathymetry", earth_bathymetry,
            "lights",     earth_lights, null)));

    model   m_cloud   = hold(model  (w, w, id, earth,   s, u.cloud,
        samplers, map_of(
            "color",      earth_color,
            "normal",     earth_normal,
            "elevation",  earth_elevation,
            "water",      earth_water,
            "water_blur", earth_water_blur,
            "cloud",      earth_cloud,
            "bathymetry", earth_bathymetry,
            "lights",     earth_lights, null)));
    
    target r_background  = target (w, w,
        wscale,         2.0f,
        clear_color,    vec4f(0.0f, 0.1f, 0.2f, 1.0f),
        models,         a(m_purple));
    app a = hold(app(w, w, arg, &u,
        r_background,  r_background,
        on_background, orbiter_background,
        on_interface,  orbiter_interface));
    auto_free();
    return run(a);
}

define_class(Audrey, shader)

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

void Purple_init(Purple w) {
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

define_class(Purple, shader)
define_enum(PurpleSurface)

define_class(Earth, shader)
define_class(Ocean, Earth)
define_class(Cloud, Earth)
define_class(Orbiter, PBR)
define_enum(EarthSurface)

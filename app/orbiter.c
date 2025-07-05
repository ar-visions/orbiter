
#include <import>
#include <math.h>

static window main_window;

object orbiter_window_mouse(orbiter a, event e) {
    return null;
}

// background update sub
object orbiter_space_update(orbiter a, background sc) {
    a->time += (4 * 0.0044f) - (a->w->debug_value * 0.04);

    // step 1: apply Earth's axial tilt
    float tilt_deg  = -23.44f;
    float tilt_rad  = radians(tilt_deg);
    vec4f tilt_axis = vec4f(1.0f, 0.0f, 0.0f, tilt_rad); // tilt around X axis
    quatf q_tilt    = quatf(&tilt_axis);
    mat4f m_tilt    = mat4f(&q_tilt);

    // step 2: spin Earth around Y axis *in its local (tilted) space*
    vec4f spin_axis = vec4f(0.0f, 1.0f, 0.0f, radians(a->time));
    quatf q_spin    = quatf(&spin_axis);
    mat4f m_spin    = mat4f(&q_spin);

    if (a->earth && a->ocean && a->cloud) {
        // final model matrix: spin after tilt
        a->earth->model = mat4f_mul(&m_tilt, &m_spin);
        a->ocean->model = a->earth->model;
        a->cloud->model = a->earth->model;

        a->earth->time = a->time;
        a->ocean->time = a->time;
        a->cloud->time = a->time;
    }

    if (a->purple) {
        a->purple->model = mat4f_mul(&m_tilt, &m_spin);
        a->purple->time = a->time;
    }
    return null;
}

object orbiter_iris_update(orbiter a, scene sc) {
    window w = a->w;
    vec3f   eye         = vec3f   (0.0f,  4.0f, 0.0f);
    vec3f   target      = vec3f   (0.0f,  0.0f,  0.0f);
    vec3f   up          = vec3f   (0.0f,  0.0f,  -1.0f);

    static float sequence = 0;
    sequence += 0.0022f;
    static float m_sequence = 0;
    m_sequence += 0.00022f;


    Orbiter orb          = a->orbiter;

    vec4f v              = vec4f(0.0f, 1.0f, 0.0f, radians(30.0));
    quatf q              = quatf(&v);
    orb->model           = mat4f_ident();
    orb->model           = mat4f_rotate(&orb->model, &q);

    orb->pos              = vec4f(&eye);
    //orb->pos2            = vec4f(0.22, 0.22, 0.22, 1.0);

    orb->proj            = mat4f_perspective(radians(40.0f), 1.0f, 0.1f, 100.0f);
    orb->view            = mat4f_look_at    (&eye, &target, &up);
    orb->moment          = m_sequence;
    orb->moment_angle    = 0.5f;
    orb->moment_amount   = 10.0f;
    orb->pos_radius      = vec4f(0.92, 0.0, -0.09, 0.5f);
    orb->normal_falloff  = vec4f(0.0,  1.0, 0.0, 0.8f);
    orb->color_intensity = vec4f(0.6,  0.3, 1.0, 4.0f);

    if (orb->moment_angle > M_PI) orb->moment_angle = orb->moment_angle - M_PI;

    //a->aa->model = mat4f_ident();
    // rotate 90 deg X
    //vec4f rotation_axis_angle = vec4f(1.0f, 0.0f, 0.0f, radians(90.0f));
    //quatf rotation_quat       = quatf(&rotation_axis_angle);
    //a->aa->model = mat4f_rotate(&a->aa->model, &rotation_quat);
    

    /// move down
    vec3f tr              = vec3f(0.0f, 0.5f, 0.0f);
    if (a->au) {
        a->au->model          = mat4f_translate(&a->au->model, &tr);
        a->au->view           = mat4f_ident();
        a->au->proj           = mat4f_ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        a->au->v_model        = orb->model;
        a->au->v_view         = orb->view;
        a->au->v_proj         = orb->proj;
        a->au->time           = sequence;
        a->au->thrust         = 1.00;
        a->au->magnitude      = 1.00;
        a->au->colors         = 0.22;
        a->au->halo_attenuate = 0.44;
        a->au->halo_space     = 0.44;
        a->au->halo_separate  = 0.11;
        a->au->halo_thickness = 0.22;

        sk cv = a->au->cv;
        clear     (cv, string("#111"));
        static float x = 0;
        static float y = 0;

        x += 1;
        if (x > 800) { 
            y += 1;
            x = 0.0;
        } 
        
        rect_to   (cv, x, y, 400, 400);
        fill_color(cv, string("#0af"));
        draw_fill (cv);
        sk_sync();
    }

    return null;
}

/*

from .ason file:

map orbiter_render(orbiter a, window arg) {
    return m(
        "space", background(
            render_scale,   1.0f, // applies to render of this frame buffer, however not the UX elements within; those are 1:1 pixel ratio
            models,         a->models,
            frost,          true,
            clear_color,    vec4f(0.0f, 0.1f, 0.2f, 1.0f),
            elements,       m(
                "main", pane(elements, m(
                    "editor",  editor(content, f(string, "orbiter editor..."))
                )),
                "debug", pane(elements, m(
                    "iris",    scene(
                        models,       a->orbiter_scene,
                        clear_color,  vec4f(0.0, 0.0, 0.0, 0.0),
                        render_scale, 4.0f)
                ))
            )
        ));
}
*/

object orbiter_dbg_exit(orbiter a, dbg debug) {
    print("debug exit");
    return null;
}

object orbiter_dbg_crash(orbiter a, cursor cur) {
    print("crash at %o:%i:%i", cur->source, cur->line, cur->column);
    return null;
}

object orbiter_dbg_break(orbiter a, cursor cur) {
    print("breakpoint at %o:%i:%i", cur->source, cur->line, cur->column);
    return null;
}

object orbiter_dbg_stdout(orbiter a, iobuffer b) {
    fwrite(b->bytes, 1, b->count, stdout);
    return null;
}

object orbiter_dbg_stderr(orbiter a, iobuffer b) {
    fwrite(b->bytes, 1, b->count, stderr);
    return null;
}

none orbiter_init(orbiter a) {

    /*a->model = mdl(uri, path("gemma-7b.Q4_K_M.gguf"));
    array startup = a(
        string("hi, we are going to show you some code first, then we'll ask a question; now respond with ok $"),
        path("/src/A/test/a-test.c"));
    a->context  = ctx(model, a->model, content, startup);
    string res  = query(a->context, string("what language was this? respond the language and then $"));
    string res2 = query(a->context, string("what did i just say about you?  1 or 2 words max"));
    */

    //print("res = %o, res2 = %o", res, res2);

    // lets test the binding api 
    //path location = hold(f(path, "/src/A/debug/test/a-test"));
    //path src      = hold(f(path, "/src/A/test/a-test.c"));
    //a->debug    = dbg(
    //    location,  location,
    //    target,    a);
    //set_breakpoint(a->debug, src, 19, 0);
    //set_breakpoint(a->debug, src, 20, 0);
    /*start(a->debug);
    for (;;) {
        usleep(10000);
    }*/

    trinity t      = a->t = trinity();

    int     width  = a->width  ? a->width  : 1200;
    int     height = a->height ? a->height : 1200;

    a->w = window(
        t, t, title, string("orbiter-canvas"), // space_update must be 1) registered and 2) called every frame
        width, width, height, height);

    /*
    a->purple_gltf    = hold(a->earth_gltf);

    a->purple_res = hold(a(
        hold(image(uri, f(path, "textures/purple-color-8192x4096.png"))),
        hold(image(uri, f(path, "textures/purple-cloud-8192x4096.png")))));
    a->purple  = Purple  (t, t, name, string("purple"));
    a->earth   = Earth   (t, t, name, string("earth"));
    a->ocean   = Ocean   (t, t, name, string("ocean"));
    a->cloud   = Cloud   (t, t, name, string("cloud"));
    a->au      = Audrey  (t, t, name, string("audrey"));

    a->orbiter_gltf  = read(f(path, "models/flower88882.gltf"), typeid(Model), null);
    a->env           = image(uri, form(path, "images/forest.exr"), surface, Surface_environment);
    a->orbiter_scene = a(model(w, w, id, a->orbiter_gltf, s, a->orbiter, samplers, m("environment", a->env)));

    a->models = a(a->earth_model, a->ocean_model, a->cloud_model);
    */

    /// create inspiring effect with au shader
    initialize(a, a->w);
}
 
none orbiter_dealloc(orbiter a) { }

int main(int argc, cstrs argv) {
    orbiter a = hold(orbiter(argv)); // initial compose must happen in init, and run will be simplified
    return run(a);
}



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

void Audrey_init(Audrey a) {
    trinity t = a->t;
    int cv_size = 128;

    a->cv   = sk(
        t, t, format, Pixel_rgba8, 
        width, cv_size, height, cv_size);

    a->core = sk(
        t, t, format, Pixel_rgba8, 
        width, cv_size, height, cv_size);
    
    a->beam = sk(
        t, t, format, Pixel_rgba8, 
        width, cv_size, height, cv_size);

    clear     (a->core, string("#0000"));
    
    /// outter color
    rect_to   (a->core, 0, 0, 800, 800);
    fill_color(a->core, string("#f00"));
    draw_fill (a->core);

    /// inner core
    arc       (a->core, 400.0f, 400.0f, 100.0f, 0.0f, 360.0f);
    fill_color(a->core, string("#a0f"));
    draw_fill (a->core);

    /// core mantle!
    arc       (a->core, 400.0f, 400.0f, 40.0f, 0.0f, 360.0f);
    fill_color(a->core, string("#aff"));
    draw_fill (a->core);

    clear     (a->beam, string("#0000"));
    rect_to   (a->beam, 0, 0, 800, 800);
    fill_color(a->beam, string("#00f"));
    draw_fill (a->beam);
    arc       (a->beam, 400.0f, 400.0f, 40.0f, 0.0f, 360.0f);
    fill_color(a->beam, string("#aff"));
    draw_fill (a->beam);
 
    sk_sync();
}

define_class(orbiter, app)

define_class(Purple, shader)
define_enum(PurpleSurface)

define_class(Audrey, shader)
define_class(Earth, shader)

define_class(Ocean, Earth)
define_class(Cloud, Earth)
define_class(Orbiter, PBR)
define_enum(EarthSurface)

define_enum(Language)
define_class(editor, element)
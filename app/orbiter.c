
#include <import>
#include <math.h>

static window main_window;

typedef struct Shaders {
    window  w;
    //Audrey  au;
    UVQuad  screen;
    sk      canvas;
} Shaders;

void orbiter_mousemove() {
}

void orbiter_frame(Shaders* u) {
    sk cv = u->canvas;
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
    rect      (cv, x, y, 400, 400);
    fill_color(cv, string("#0af"));
    draw_fill (cv, false);
    sync      (cv);
    output_mode(cv, true);
}

element orbiter_render() {
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
        .canvas = sk(
            t, t, format, Pixel_rgba8, surface, Surface_color,
            width, width, height, height)
    };

    /// create agent model with texture associated to render
    window  w         = u.w;
    model   m_view    = model  (w, w, samplers, a(u.canvas->tx));
    //model   m_au      = model  (w, w, s, u.au, samplers, a(u.beam));
    render  r_view    = render (w, w, clear_color, vec4f(0.0, 0.5, 0.5, 1.0), models, a(m_view));

    w->list = a(r_view);
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

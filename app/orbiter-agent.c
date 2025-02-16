
#include <import>

static int n_parts = 3;

/// goal: draw hinge first, a movable armature
int something();

int main(int argc, char *argv[]) {
    A_start();

    int i = something();

    mat4f a      = mat4f(null);
    mat4f b      = translate(mat4f(null), vec3f(1.0f, 1.0f, 1.0f));
    mat4f r      = mul(a, b);

    path    gltf_file = path("models/hinge.gltf");
    Model   hinge     = read(gltf_file, typeid(Model)); // <- seems like completely valid data loaded into hinge from our .gltf file.  we want to construct a 'pipeline' based on this!
    vertex  parts     = A_alloc(typeid(vertex), n_parts, true);

    // Bottom face edges <- this is a triangle only.. just test data for a pipeline
    // we want to draw the entire hinge structure we showed 
    parts[0] = (struct vertex) { .pos = {  0.0f, -0.5f,  0.5f } };
    parts[1] = (struct vertex) { .pos = { -0.5f,  0.5f,  0.5f } };
    parts[2] = (struct vertex) { .pos = {  0.5f,  0.5f,  0.5f } };

    trinity  t = trinity();
    window   w = window(t, t, title, string("orbiter"), width, 800, height, 800);
    shader   s = shader(t, t, vert, string("cube.vert"), frag, string("cube.frag"));
    pipeline cube = pipeline(t, t, w, w, shader, s, read, parts);
    push(w, cube);

    loop(w);
    return  0;
}
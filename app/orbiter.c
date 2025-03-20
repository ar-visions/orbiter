
#include <import>

int main(int argc, cstr argv[]) {
    /// initialize A-type
    A_start();

    /// construct path-with-cstr [ called from C11 _Generic ]
    /// constructors are restricted to 1-arg, because when two or more, they are
    /// variadic prop-name, "arg-value", ...
    path     gltf      = path     ( "models/flower88.4.gltf" );
    Model    orbiter   = read     ( gltf, typeid(Model) ); /// data models are upper-case
    /// orbiter contains data of all structure/fields in glTF file

    /// initialize
    trinity  t = trinity  ( );
    window   w = window   ( t, t, title, string("orbiter"), width, 800, height, 800 );

    shader    pbr            = shader   ( t, t, name,  string("pbr"));
    Node      id_flower      = find     ( orbiter,  "flower");
    Primitive pr_containment = primitive(id_flower, orbiter, "flower1");
    Node      id_body        = find     ( orbiter,  "body");
    Primitive body_outside1  = primitive(id_body, orbiter, "body1");
    Primitive body_outside2  = primitive(id_body, orbiter, "body2");
    Primitive body_outside3  = primitive(id_body, orbiter, "body3");
    Primitive body_outside4  = primitive(id_body, orbiter, "body4");
    array    flower_shaded   = array_of (
        part(id, pr_containment, s, pbr), null );
    
    array    body_shaded     = array_of (
        part(id, body_outside1, s, pbr), 
        part(id, body_outside2, s, pbr), 
        part(id, body_outside3, s, pbr), 
        part(id, body_outside4, s, pbr), null );

    node     flower    = node     ( id, id_flower, parts, flower_shaded );
    node     body      = node     ( id, id_body,   parts, body_shaded );

    image sampler     = image(uri, form(path, "images/ion-orbiter.png"),       surface, Surface_color);
    image environment = image(uri, form(path, "images/qwantani_night_4k.exr"), surface, Surface_environment);
    World world       = World();

    // give us top level 
    // give us ALL the method NAMES we need for setting up proj and view
    mat4f_set_identity(&world->model);
    mat4f_set_identity(&world->view);
    mat4f_set_identity(&world->proj);

    /// create model, describing the nodes shade
    /// debug sampler creation, uniforms
    model    agent     = model    ( t,        t,
                                    w,        w,
                                    id,       orbiter, 
                                    samplers, a(sampler, environment, null),
                                    uniforms, a(world, null),
                                    nodes,    a(body, flower, null) );
    
    /// push this model to our scene, going on in the window
    push(w,  agent);

    /// these variadic macros invoke from the [f]unction-[t]able we expand polymorphic into
    /// we may meta-describe the type (AType::meta), and its various members (type_member_t::meta)
    /// from a __typeof__ (end-member; Type _ft* _f)

    return loop(w);
}
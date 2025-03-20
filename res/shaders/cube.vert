#include <import>

// these come from the vbo as read in from gltf direct
// we let gltf dictate what our vertex is; we export this direct with controls over that
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 tangents;

void main() {
    gl_Position = vec4(pos, 1.0);
}
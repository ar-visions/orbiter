#include <blur-v>

layout(location = 0) in  vec3 pos;
layout(location = 1) in  vec2 uv;

layout(location = 0) out vec2 v_uv;

void main() {
    mat4 m         = quad.proj * quad.view * quad.model;
    v_uv           = uv;
    gl_Position    = m * vec4(pos, 1.0);
}

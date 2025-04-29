#include <uv-quad>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    vec4 cv = texture(tx_color, v_uv);
    fragColor = vec4(v_uv.x, cv.x, v_uv.y, 1.0);
}

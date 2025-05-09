#include <uv-quad>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    fragColor = vec4(1.0);
    /*
    vec4 cv = texture(tx_color, v_uv);
    fragColor = cv;*/
}

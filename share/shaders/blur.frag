#include <blur>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

const int   kernel_size = 15;     // should be odd: 5, 7, 9, ...
const float sigma = 6.0;

float gaussian(float x, float sigma) {
    const float PI = 3.14159265359;
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * PI) * sigma);
}

void main() {
    fragColor = vec4(1.0);/*
    vec2  uv         = v_uv;
    int   w          = textureSize(tx_color, 0).x;
    vec2  offset     = vec2(1.0 / float(w), 0.0); // horizontal
    float weight_sum = gaussian(0.0, sigma);
    vec4  result     = texture(tx_color, uv) * weight_sum;

    for (int i = 1; i <= kernel_size / 2; ++i) {
        float w      = gaussian(float(i), sigma);
        vec2  shift  = offset * float(i);
        result      += texture(tx_color, uv + shift) * w;
        result      += texture(tx_color, uv - shift) * w;
        weight_sum  += 2.0 * w;
    }

    fragColor = result / weight_sum;*/
}
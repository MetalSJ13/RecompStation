#version 440
// Moving background "Color flow" (approved 2026-11-27): large, soft pools of
// color that drift slowly and blend into each other. The pools are the chosen
// color and two neighbours of it around the color wheel. Drawn by
// WaveBackground.qml (scene "flow").

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;          // seconds, already multiplied by the motion speed
    vec2 resolution;
    vec4 waveColor;      // the Color setting
    vec4 topColor;
    vec4 bottomColor;
    float pixel;
};

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Turns a color around the color wheel by `angle` radians.
vec3 hueShift(vec3 c, float angle) {
    const vec3 k = vec3(0.57735);
    float cosA = cos(angle);
    return c * cosA + cross(k, c) * sin(angle) + k * dot(k, c) * (1.0 - cosA);
}

float pool(vec2 q, vec2 center, float radius) {
    vec2 d = q - center;
    return exp(-dot(d, d) / (radius * radius));
}

void main() {
    vec2 uv = qt_TexCoord0;
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 q = vec2(uv.x * aspect, uv.y);
    float t = time * 0.12;
    vec3 base = clamp(waveColor.rgb, 0.0, 1.0);
    vec3 a = base;
    vec3 b = clamp(hueShift(base, 0.75), 0.0, 1.0);
    vec3 c = clamp(hueShift(base, -0.85), 0.0, 1.0);
    vec2 pa = vec2(aspect * (0.30 + 0.18 * sin(t * 0.9)), 0.35 + 0.20 * sin(t * 0.7 + 1.0));
    vec2 pb = vec2(aspect * (0.72 + 0.16 * sin(t * 0.6 + 2.0)), 0.45 + 0.22 * cos(t * 0.8));
    vec2 pc = vec2(aspect * (0.50 + 0.25 * cos(t * 0.5 + 4.0)), 0.80 + 0.15 * sin(t * 0.9 + 3.0));
    vec2 pd = vec2(aspect * (0.15 + 0.12 * cos(t * 0.7 + 5.0)), 0.85 + 0.10 * cos(t * 0.6));
    float wa = pool(q, pa, 0.42);
    float wb = pool(q, pb, 0.38);
    float wc = pool(q, pc, 0.36);
    float wd = pool(q, pd, 0.28);
    vec3 col = bottomColor.rgb * 0.8;
    col += a * wa * 0.50 + b * wb * 0.45 + c * wc * 0.40 + a * wd * 0.30;
    // Darker toward the top, where the menu bar sits.
    col *= mix(0.7, 1.0, uv.y);
    col += (hash(uv * resolution) - 0.5) / 255.0;
    fragColor = vec4(min(col, vec3(1.0)), 1.0) * qt_Opacity;
}

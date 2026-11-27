#version 440
// Moving background "Twinkling stars" (approved 2026-11-27): square pixel
// stars in three depths, each fading in and out in small steps at its own
// pace, with a shooting star now and then. Some stars take the chosen color;
// the rest stay white. Drawn by WaveBackground.qml (scene "stars").

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;          // seconds, already multiplied by the motion speed
    vec2 resolution;     // rendered size in pixels
    vec4 waveColor;      // the Color setting
    vec4 topColor;
    vec4 bottomColor;
    float pixel;         // rendered pixels per logical pixel
};

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 starLayer(vec2 lp, float size, float blocks, float seed) {
    vec2 cellId = floor(lp / size);
    if (hash(cellId + seed) > 0.55)
        return vec3(0.0);
    vec2 pos = floor(vec2(hash(cellId + seed + 1.3), hash(cellId + seed + 2.7)) * (size - 8.0) + 4.0);
    vec2 d = floor(lp - cellId * size) - pos;
    if (d.x < 0.0 || d.y < 0.0 || d.x >= blocks || d.y >= blocks)
        return vec3(0.0);
    float phase = hash(cellId + seed + 3.1) * 6.2832;
    float rate = 0.6 + hash(cellId + seed + 5.7) * 1.8;
    float wave = 0.5 + 0.5 * sin(time * rate + phase);
    float brightness = 0.2 + 0.8 * floor(wave * 4.0 + 0.5) / 4.0;   // four steps, like old hardware
    vec3 tint = hash(cellId + seed + 4.9) > 0.72 ? mix(waveColor.rgb, vec3(1.0), 0.3) : vec3(0.92, 0.94, 1.0);
    return tint * brightness;
}

float shootingStar(vec2 q, float aspect, float px) {
    float period = 9.0;
    float n = floor(time / period);
    float t = mod(time, period);
    if (t > 1.1 || hash(vec2(n, 7.0)) > 0.75)
        return 0.0;
    float progress = t / 1.1;
    vec2 dir = normalize(vec2(1.0, 0.38));
    vec2 start = vec2(hash(vec2(n, 1.0)) * aspect * 0.6, 0.05 + hash(vec2(n, 2.0)) * 0.3);
    vec2 head = start + dir * progress * 0.55;
    vec2 rel = q - head;
    float along = -dot(rel, dir);
    float perp = abs(rel.x * dir.y - rel.y * dir.x);
    float trail = 0.14;
    if (along < 0.0 || along > trail)
        return 0.0;
    return (1.0 - along / trail) * (1.0 - smoothstep(px * 0.6, px * 1.8, perp)) * sin(progress * 3.1416);
}

void main() {
    vec2 uv = qt_TexCoord0;
    float px = max(pixel, 0.5);
    vec2 lp = uv * resolution / px;                 // logical pixels
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec3 col = mix(topColor.rgb * 0.5, bottomColor.rgb * 0.45, uv.y);
    col += starLayer(lp, 29.0, 1.0, 0.0) * 0.7;
    col += starLayer(lp, 61.0, 2.0, 17.0);
    col += starLayer(lp, 127.0, 3.0, 43.0);
    col += vec3(0.95, 0.97, 1.0) * shootingStar(vec2(uv.x * aspect, uv.y), aspect, 1.0 / max(resolution.y, 1.0) * px);
    fragColor = vec4(col, 1.0) * qt_Opacity;
}

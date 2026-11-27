#version 440
// Library "Still" background patterns, approved 2026-11-27 (design canvas
// "RecompStation retro backgrounds", option B): CRT glow, dithered sunset,
// pixel starfield and circuit board. Drawn by RetroBackground.qml into a
// cached layer, so it runs only when the size, the colors or the pattern
// change, never while the Library scrolls.
//   pattern 1 = CRT glow, 2 = dithered sunset, 3 = pixel starfield,
//   4 = circuit board (0, the gradient, is drawn by QML).
// Sizes are in logical pixels (times `pixel`, the screen's pixel ratio), so
// the patterns look the same on every screen.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 resolution;     // rendered size in device pixels
    float pattern;
    float pixel;         // device pixels per logical pixel
    vec4 accent;
    vec4 topColor;
    vec4 bottomColor;
};

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// 4x4 ordered dither threshold, 0..1.
float bayer4(vec2 c) {
    const float m[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                                  3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
    ivec2 i = ivec2(mod(c, 4.0));
    return (m[i.x + i.y * 4] + 0.5) / 16.0;
}

vec3 crtGlow(vec2 uv, vec2 p, float px, float aspect) {
    // The whole window is the screen of an old tube television: a soft glow
    // in the middle, corners that fall off like curved glass, fine
    // scanlines and a faint phosphor stripe. (A smaller tube drawn inside
    // the window looked like a frame around nothing; changed 2026-11-27.)
    vec2 c = (uv - 0.5) * vec2(aspect, 1.0);
    vec3 col = bottomColor.rgb * 0.95 + accent.rgb * 0.12 * exp(-dot(c, c) * 2.5);
    vec2 halfSize = vec2(0.5 * aspect, 0.5);
    float r = 0.08;
    vec2 q = abs(c) - halfSize + r;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
    col *= mix(0.45, 1.0, smoothstep(0.0, 0.18, -d));
    float column = mod(floor(p.x / px), 3.0);
    vec3 phosphor = column < 1.0 ? vec3(1.06, 0.97, 0.97)
                  : column < 2.0 ? vec3(0.97, 1.06, 0.97) : vec3(0.97, 0.97, 1.06);
    col *= phosphor;
    col *= mod(floor(p.y / px), 3.0) < 1.0 ? 0.80 : 1.0;
    return col;
}

vec3 ditheredSunset(vec2 p, float px, float aspect) {
    float cell = 2.0 * px;
    vec2 b = floor(p / cell);
    vec2 buv = (b * cell + 0.5 * cell) / resolution;
    // Dusk rather than full sunset: dark, muted colors so covers stay the
    // brightest thing on screen (toned down 2026-11-27 after the first test).
    const vec3 c0 = vec3(0.059, 0.039, 0.118);
    const vec3 c1 = vec3(0.118, 0.071, 0.212);
    const vec3 c2 = vec3(0.227, 0.110, 0.275);
    const vec3 c3 = vec3(0.369, 0.173, 0.290);
    // Four colors only, blended by ordered dithering, as 16-bit games did.
    float t = clamp(buv.y / 0.84, 0.0, 1.0);
    t = t * t * 0.6 + t * 0.4;
    float level = floor(t * 3.0 + bayer4(b));
    vec3 col = level < 0.5 ? c0 : level < 1.5 ? c1 : level < 2.5 ? c2 : c3;
    // A striped sun sitting on the horizon.
    vec2 s = vec2((buv.x - 0.72) * aspect, buv.y - 0.80);
    if (length(s) < 0.085 && !(s.y > 0.0 && fract(s.y * 55.0) < 0.15 + s.y * 6.0))
        col = vec3(0.722, 0.447, 0.282);
    // Two rows of blocky hills.
    float x = buv.x * aspect * 9.0;
    float n = mix(hash(vec2(floor(x), 1.0)), hash(vec2(floor(x) + 1.0, 1.0)), fract(x));
    if (buv.y > 0.88 - 0.06 * n)
        col = vec3(0.078, 0.047, 0.122);
    float x2 = buv.x * aspect * 5.0 + 3.7;
    float n2 = mix(hash(vec2(floor(x2), 2.0)), hash(vec2(floor(x2) + 1.0, 2.0)), fract(x2));
    if (buv.y > 0.94 - 0.04 * n2)
        col = vec3(0.047, 0.027, 0.078);
    return col;
}

// One layer of square stars: at most one per `size`-pixel cell.
vec3 starLayer(vec2 p, float px, float size, float blocks, float seed) {
    vec2 lp = p / px;
    vec2 cellId = floor(lp / size);
    if (hash(cellId + seed) > 0.55)
        return vec3(0.0);
    vec2 pos = floor(vec2(hash(cellId + seed + 1.3), hash(cellId + seed + 2.7)) * (size - 8.0) + 4.0);
    vec2 d = floor(lp - cellId * size) - pos;
    if (d.x < 0.0 || d.y < 0.0 || d.x >= blocks || d.y >= blocks)
        return vec3(0.0);
    float brightness = 0.45 + 0.55 * hash(cellId + seed + 3.1);
    vec3 tint = hash(cellId + seed + 4.9) > 0.72 ? mix(accent.rgb, vec3(1.0), 0.35) : vec3(0.92, 0.94, 1.0);
    return tint * brightness;
}

vec3 pixelStarfield(vec2 uv, vec2 p, float px) {
    vec3 col = mix(topColor.rgb * 0.5, bottomColor.rgb * 0.45, uv.y);
    col += starLayer(p, px, 29.0, 1.0, 0.0) * 0.7;
    col += starLayer(p, px, 61.0, 2.0, 17.0);
    col += starLayer(p, px, 127.0, 3.0, 43.0);
    return col;
}

vec3 circuitBoard(vec2 uv, vec2 p, float px) {
    vec3 col = mix(topColor.rgb * 0.75, bottomColor.rgb, uv.y);
    float tile = 56.0;
    vec2 lp = p / px;
    vec2 id = floor(lp / tile);
    vec2 f = lp - (id + 0.5) * tile;          // logical pixels from the tile's center
    float right = step(0.45, hash(id + 0.3));
    float left = step(0.45, hash(id - vec2(1.0, 0.0) + 0.3));
    float down = step(0.5, hash(id + 0.7));
    float up = step(0.5, hash(id - vec2(0.0, 1.0) + 0.7));
    float w = 1.0;
    float trace = 0.0;
    if (right > 0.5 && f.x >= 0.0 && abs(f.y) <= w) trace = 1.0;
    if (left > 0.5 && f.x <= 0.0 && abs(f.y) <= w) trace = 1.0;
    if (down > 0.5 && f.y >= 0.0 && abs(f.x) <= w) trace = 1.0;
    if (up > 0.5 && f.y <= 0.0 && abs(f.x) <= w) trace = 1.0;
    float links = right + left + up + down;
    // A solder pad where a trace ends; now and then a chip.
    float pad = links == 1.0 ? 1.0 - step(4.0, length(f)) : 0.0;
    float chipBox = max(abs(f.x), abs(f.y));
    bool chip = hash(id + 5.1) > 0.93;
    col = mix(col, mix(col, accent.rgb, 0.45), trace * 0.75);
    col = mix(col, accent.rgb, pad * 0.6);
    if (chip && chipBox < 12.0)
        col = chipBox > 10.5 ? mix(col, accent.rgb, 0.45) : bottomColor.rgb * 1.25;
    return col;
}

void main() {
    vec2 uv = qt_TexCoord0;
    vec2 p = uv * resolution;
    float px = max(pixel, 1.0);
    float aspect = resolution.x / max(resolution.y, 1.0);
    int k = int(pattern + 0.5);
    vec3 col = mix(topColor.rgb, bottomColor.rgb, uv.y);
    if (k == 1)
        col = crtGlow(uv, p, px, aspect);
    else if (k == 2)
        col = ditheredSunset(p, px, aspect);
    else if (k == 3)
        col = pixelStarfield(uv, p, px);
    else if (k == 4)
        col = circuitBoard(uv, p, px);
    fragColor = vec4(col, 1.0) * qt_Opacity;
}

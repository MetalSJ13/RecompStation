#version 440
// Moving background "Floating lights" (approved 2026-11-27, from the user's
// reference picture): out-of-focus lights in one color, gathered toward the
// bottom over a soft glow and thinning out upward. Far lights are small and
// sharp, near ones larger and softer; all drift slowly upward and gently
// brighten and dim. Drawn by WaveBackground.qml (scene "lights").

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

// One depth of lights. q is in screen-height units (x scaled by the aspect).
// cell: spacing, radius: largest light, soft: how blurred the edge is,
// rise: upward speed, strength: overall brightness.
vec3 lightLayer(vec2 q, float cell, float radius, float soft, float rise, float strength, float seed, vec3 color) {
    vec2 moved = q + vec2(0.0, time * rise);
    vec2 base = floor(moved / cell);
    vec3 sum = vec3(0.0);
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 id = base + vec2(float(i), float(j));
            float h = hash(id + seed);
            if (h > 0.7)
                continue;
            vec2 center = (id + vec2(hash(id + seed + 1.7), hash(id + seed + 3.3))) * cell;
            center.x += sin(time * 0.25 + h * 6.2832) * cell * 0.25;
            float r = radius * (0.55 + 0.45 * hash(id + seed + 5.1));
            float d = length(moved - center) / r;
            if (d > 1.0)
                continue;
            float disc = 1.0 - smoothstep(1.0 - soft, 1.0, d);
            disc += 0.18 * smoothstep(0.7, 0.95, d) * disc;        // a faint brighter rim
            float tone = hash(id + seed + 7.9);
            vec3 c = tone < 0.35 ? color * 0.45
                   : tone < 0.75 ? mix(color, vec3(1.0), 0.25) : mix(color, vec3(1.0), 0.55);
            float twinkle = 0.75 + 0.25 * sin(time * (0.7 + h) + h * 12.0);
            // Where the light is on screen: bright near the bottom, fading upward.
            float screenY = center.y - time * rise;
            float fade = mix(0.08, 1.0, pow(clamp(screenY, 0.0, 1.0), 1.6));
            sum += c * disc * twinkle * fade;
        }
    }
    return sum * strength;
}

void main() {
    vec2 uv = qt_TexCoord0;
    float aspect = resolution.x / max(resolution.y, 1.0);
    vec2 q = vec2(uv.x * aspect, uv.y);
    vec3 color = waveColor.rgb;
    vec3 col = bottomColor.rgb * 0.35;
    // The glow along the bottom.
    float fromBottom = 1.0 - uv.y;
    col += color * (0.30 * exp(-fromBottom * 3.2));
    col += lightLayer(q, 0.035, 0.007, 0.25, 0.006, 0.55, 0.0, color);
    col += lightLayer(q, 0.075, 0.018, 0.45, 0.010, 0.50, 11.0, color);
    col += lightLayer(q, 0.14, 0.034, 0.75, 0.016, 0.42, 29.0, color);
    // A touch of noise keeps the soft glow from banding.
    col += (hash(uv * resolution) - 0.5) / 255.0;
    fragColor = vec4(min(col, vec3(1.0)), 1.0) * qt_Opacity;
}

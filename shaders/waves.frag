#version 440
// Library "Waves" background: two ribbons of thin glowing lines, each a family
// of lines interpolated between two independently moving curves, so the
// ribbon twists and pinches as the curves cross. Ported from the prototype
// reviewed on 2026-11-23. Drawn by a ShaderEffect in WaveBackground.qml.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;          // seconds, already multiplied by the motion speed
    vec2 resolution;     // rendered size in pixels (sets the line width)
    vec4 waveColor;
    vec4 topColor;
    vec4 bottomColor;
};

float curve(float x, float t, vec4 p) {
    return p.x + 0.09 * sin(x * p.y + t * p.z)
               + 0.05 * sin(x * p.y * 2.3 - t * p.z * 1.4 + p.w)
               + 0.02 * sin(x * p.y * 5.1 + t * p.z * 0.7 + p.w * 2.0);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 ribbon(float x, float y, float t, vec4 pa, vec4 pb, float strength, float px, vec3 color) {
    float a = curve(x, t, pa);
    float b = curve(x, t, pb);
    float lo = min(a, b);
    float hi = max(a, b);
    float inside = smoothstep(lo - px * 2.0, lo + px * 2.0, y)
                 * (1.0 - smoothstep(hi - px * 2.0, hi + px * 2.0, y));
    float spread = hi - lo;
    vec3 col = color * inside * (0.05 + 0.10 * (1.0 - clamp(spread * 6.0, 0.0, 1.0))) * strength;
    float core = 0.0;
    float glow = 0.0;
    for (int i = 0; i < 14; i++) {
        float fi = float(i) / 13.0;
        float d = abs(y - mix(a, b, fi));
        float w = 0.30 + 0.70 * fi;
        core += (1.0 - smoothstep(0.0, px * 1.4, d)) * w;
        glow += exp(-d / (px * 7.0)) * w;
    }
    col += mix(color, vec3(1.0), 0.55) * core * 0.30 * strength;
    col += color * glow * 0.06 * strength;
    return col;
}

void main() {
    vec2 uv = vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y);
    float aspect = resolution.x / max(resolution.y, 1.0);
    float x = uv.x * aspect;
    float y = uv.y;
    float px = 1.0 / max(resolution.y, 1.0);
    float t = time;
    vec3 color = waveColor.rgb;

    vec3 col = mix(bottomColor.rgb, topColor.rgb, smoothstep(0.0, 1.0, y));
    float hy = (y - 0.48) * 3.2;
    col += color * exp(-hy * hy) * (0.65 + 0.35 * sin(x * 0.7 + t * 0.05)) * 0.10;
    col += ribbon(x, y, t * 0.8, vec4(0.30, 0.9, 0.22, 4.0), vec4(0.36, 1.5, 0.31, 1.0), 0.45, px, color);
    col += ribbon(x, y, t, vec4(0.46, 1.3, 0.35, 0.0), vec4(0.54, 1.05, 0.28, 2.1), 1.0, px, color);

    vec2 c = uv - 0.5;
    col *= 1.0 - dot(c, c) * 0.55;
    // Dither so the dark gradient does not band.
    col += (hash(gl_FragCoord.xy + fract(t)) - 0.5) / 255.0;
    fragColor = vec4(col, 1.0) * qt_Opacity;
}

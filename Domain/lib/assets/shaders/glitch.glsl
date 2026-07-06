#pragma vertex
#version 450 core
layout (location = 0) in vec2 aPos; // [-0.5, 0.5] quad
layout (location = 1) in vec2 aUV;  // [0,1] quad UVs

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2 uSize = vec2(1.0, 1.0);    // sprite width/height in world units
uniform vec2 uPivot = vec2(0.5, 0.5);   // pivot (0..1)
uniform vec2 uUVScale = vec2(1.0, 1.0);
uniform vec2 uUVOffset = vec2(0.0, 0.0);

out vec2 vTexCoord;

void main() {
    // Compute per-vertex UVs
    vTexCoord = (aUV * uUVScale) + uUVOffset;

    // Scale quad by size
    vec2 scaledPos = aPos * uSize;

    // Apply pivot offset
    scaledPos -= uSize * uPivot;

    // Transform to world space
    gl_Position = uProj * uView * uModel * vec4(scaledPos, 0.0, 1.0);
}

#pragma fragment
#version 450 core
out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uTexture;  // bound per sprite
uniform vec4 uColor = vec4(1.0); // per-sprite color multiplier
uniform float uTime;

uniform float uGlitchIntensity = 0.3; // 0 = off, 1 = maximum chaos
uniform float uGlitchSpeed = 10.0;    // how fast glitch bands cycle

float hash(float n)
{
    return fract(sin(n) * 43758.5453123);
}

void main()
{
    float t = floor(uTime * uGlitchSpeed);

    // Chop the sprite into horizontal scanline bands and jitter each band's UV.x.
    float band = floor(vTexCoord.y * 24.0);
    float bandNoise = hash(band + t);
    float shift = (bandNoise - 0.5) * 0.1 * uGlitchIntensity;
    shift *= step(0.8, hash(band * 3.1 + t)); // only some bands glitch each tick

    vec2 uv = vec2(vTexCoord.x + shift, vTexCoord.y);

    // Chromatic aberration: sample each channel at a slightly different offset.
    float caOffset = 0.01 * uGlitchIntensity;
    float r = texture(uTexture, uv + vec2( caOffset, 0.0)).r;
    vec2 gSample = texture(uTexture, uv).ga;
    float g = gSample.x;
    float a = gSample.y;
    float b = texture(uTexture, uv - vec2( caOffset, 0.0)).b;

    if (a < 0.01)
        discard;

    // Occasional bright flicker so the effect reads as unstable, not just shifted.
    float flicker = step(0.96, hash(t * 1.7)) * 0.5;

    vec3 finalRGB = vec3(r, g, b) + flicker;
    FragColor = vec4(finalRGB, a) * uColor;
}

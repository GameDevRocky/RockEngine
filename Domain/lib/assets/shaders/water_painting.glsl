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

uniform float uBleedStrength = 0.015; // how far pigment bleeds from its UV
uniform float uBleedScale = 6.0;      // size of the bleed blobs
uniform float uBleedDrift = 0.05;     // how fast wet pigment keeps drifting

uniform float uEdgeDarken = 1.5;      // pigment pooling at silhouette edges
uniform float uGrainScale = 200.0;    // paper grain frequency
uniform float uGrainStrength = 0.06;  // paper grain visibility

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Bilinearly-interpolated value noise: smooth enough to look like bleeding
// pigment rather than per-pixel static.
float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main()
{
    // Wet pigment bleeding away from its original UV, slowly drifting over time.
    vec2 bleedUV = vTexCoord + (vec2(
        noise(vTexCoord * uBleedScale + vec2(uTime * uBleedDrift, 0.0)),
        noise(vTexCoord * uBleedScale + vec2(0.0, uTime * uBleedDrift) + 7.3)
    ) - 0.5) * uBleedStrength;

    vec4 texColor = texture(uTexture, bleedUV);

    if (texColor.a < 0.01)
        discard;

    // Pigment pools and darkens where the silhouette's alpha changes fastest (its edges).
    float edge = clamp(fwidth(texColor.a) * uEdgeDarken, 0.0, 0.6);

    // Paper grain: subtle per-pixel brightness variation from the noise field.
    float grain = (noise(vTexCoord * uGrainScale) - 0.5) * uGrainStrength;

    vec3 pigment = texColor.rgb * (1.0 - edge) + grain;

    FragColor = vec4(pigment, texColor.a) * uColor;
}

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

uniform float uTime;
uniform float uWaveAmplitude = 0.05; // vertical bob, in world units
uniform float uWaveFrequency = 6.0;  // ripples across the sprite width
uniform float uWaveSpeed = 2.0;

out vec2 vTexCoord;

void main() {
    // Compute per-vertex UVs
    vTexCoord = (aUV * uUVScale) + uUVOffset;

    // Scale quad by size
    vec2 scaledPos = aPos * uSize;

    // Apply pivot offset
    scaledPos -= uSize * uPivot;

    // Ripple the surface up/down based on horizontal position and time.
    scaledPos.y += sin(aPos.x * uWaveFrequency + uTime * uWaveSpeed) * uWaveAmplitude;

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

uniform vec4 uWaterColor = vec4(0.2, 0.55, 0.85, 0.6); // tint color + tint strength
uniform float uDistortStrength = 0.02; // UV wobble amount
uniform float uDistortFrequency = 8.0;

void main()
{
    // Wobble the sample point so the texture looks like it's rippling underwater.
    vec2 distortedUV = vTexCoord + vec2(
        sin(vTexCoord.y * uDistortFrequency + uTime * 2.0),
        cos(vTexCoord.x * uDistortFrequency + uTime * 1.5)
    ) * uDistortStrength;

    vec4 texColor = texture(uTexture, distortedUV);

    if (texColor.a < 0.01)
        discard;

    // Tint toward the water color and add a moving specular-ish highlight.
    vec3 tinted = mix(texColor.rgb, uWaterColor.rgb, uWaterColor.a);
    float highlight = pow(max(sin(distortedUV.x * 20.0 + uTime * 3.0), 0.0), 8.0) * 0.5;

    FragColor = vec4(tinted + highlight, texColor.a) * uColor;
}

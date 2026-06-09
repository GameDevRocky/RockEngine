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

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);

    if (texColor.a < 0.01)
        discard;

    FragColor = texColor * uColor;
}

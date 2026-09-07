#pragma domain trail
// Declares the vertex contract this shader expects. Omitted => "sprite", so every
// existing shader keeps its meaning. The editor uses it to filter the material
// picker on a TrailRenderer and to warn (not block) on a mismatch.
//
// A trail mesh arrives PRE-POSITIONED and PRE-COLOURED: aPos is already a world
// position (TrailMesh builds the ribbon in world space), aUV already spans [0,1]
// head-to-tail, and aColor already carries the start->end colour lerp baked per
// vertex. ScenePass therefore draws trails with an IDENTITY model matrix.
//
// The extra vec4 attribute at location 2 is why trails need their own shader
// rather than borrowing sprite.glsl: the per-vertex colour is what produces the
// fade along the ribbon without a gradient texture.

#pragma vertex
#version 450 core
layout (location = 0) in vec2 aPos;    // world units
layout (location = 1) in vec2 aUV;     // [0,1] along the ribbon, [0,1] across it
layout (location = 2) in vec4 aColor;  // start->end lerp, baked per vertex

uniform mat4 uModel;   // identity for trails; present so the pass can set it uniformly
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2 uUVScale = vec2(1.0, 1.0);
uniform vec2 uUVOffset = vec2(0.0, 0.0);

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    // Remapped into the sprite's own atlas sub-rect, so a textured trail samples
    // its region instead of the whole sheet. Identity when untextured.
    vTexCoord = (aUV * uUVScale) + uUVOffset;
    vColor = aColor;
    gl_Position = uProj * uView * uModel * vec4(aPos, 0.0, 1.0);
}

#pragma fragment
#version 450 core
out vec4 FragColor;

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uTexture;
// 0 = untextured. A trail is usually a plain coloured ribbon, and this lets that
// case skip the sample entirely rather than requiring a 1x1 white texture asset
// to exist just to multiply by one.
uniform float uHasTexture = 0.0;
// Per-material tint on top of the per-vertex gradient. Declared with a GLSL
// default so Material::Validate auto-seeds it and it shows up on the material.
uniform vec4 uColor = vec4(1.0);

// Deliberately unlit, like the particle path: a trail is an effect, and lighting
// a fading additive ribbon reads as a bug rather than as depth.
void main() {
    vec4 texColor = mix(vec4(1.0), texture(uTexture, vTexCoord), uHasTexture);
    vec4 result = texColor * vColor * uColor;

    // Fully transparent fragments are the tail of every trail; discarding them
    // keeps them out of the depth buffer even if a caller forgets the depth mask.
    if (result.a < 0.002)
        discard;

    FragColor = result;
}

// Builds one row of the shadow atlas: a 1D polar depth map of the nearest
// occluder distance in every direction around a light, split into four
// 90-degree quadrants (see ShadowAtlas.hpp).
//
// Driven by ShadowAtlas::RenderLight, which sets the viewport to one quadrant
// of one row and draws every occluder segment as a screen-tall quad.

#pragma vertex
#version 450 core
layout (location = 0) in vec2  aPos;    // occluder endpoint, world space
layout (location = 1) in float aSide;   // -1 / +1, expands the segment to a quad

uniform vec2  uLightPos;
uniform float uRange;
uniform int   uQuadrant;   // 0 = +X, 1 = +Y, 2 = -X, 3 = -Y

out float vDist;

// ─── SHARED WITH sprite.glsl ─────────────────────────────────────────────────
// Rotates a light-relative offset so the quadrant's forward axis becomes .x.
// sprite.glsl samples the atlas with the identical mapping. If these two ever
// disagree, shadows come out rotated by 90 degrees at the quadrant seams --
// keep the two copies byte-identical.
vec2 RotateToQuadrant(vec2 d, int q) {
    if (q == 0) return vec2( d.x,  d.y);
    if (q == 1) return vec2( d.y, -d.x);
    if (q == 2) return vec2(-d.x, -d.y);
    return              vec2(-d.y,  d.x);
}
// ─────────────────────────────────────────────────────────────────────────────

void main() {
    vec2 d  = aPos - uLightPos;
    vec2 dq = RotateToQuadrant(d, uQuadrant);

    // Normalized distance, interpolated perspective-correctly along the segment
    // because w below is the quadrant's forward depth.
    vDist = length(d) / uRange;

    // A real 90-degree perspective face, done by hand:
    //   ndc.x = dq.y / dq.x  ->  spans exactly [-1,1] across the quadrant
    //   w = dq.x             ->  anything behind the face (dq.x <= 0) is clipped
    //   ndc.y = +/-1         ->  the quad covers the full 1-pixel row height
    gl_Position = vec4(dq.y, aSide * dq.x, 0.0, dq.x);
}

#pragma fragment
#version 450 core
in float vDist;
out vec4 FragColor;

void main() {
    // Blended with glBlendEquation(GL_MIN), so the nearest occluder along each
    // direction survives without a depth buffer or any sorting.
    FragColor = vec4(vDist, 0.0, 0.0, 1.0);
}

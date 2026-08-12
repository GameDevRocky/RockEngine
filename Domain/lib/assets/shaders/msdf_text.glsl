#pragma domain text
// Declares the vertex contract this shader expects. Omitted => "sprite", so every
// existing shader keeps its meaning. The editor uses it to filter the material
// picker on a TextRenderer and to warn (not block) on a mismatch.
//
// A text mesh arrives PRE-POSITIONED: aPos is already a glyph corner in
// text-local world units and aUV is already that glyph's rect in the MSDF atlas.
// TextRenderer::OverrideUniforms pushes uSize=(1,1), uPivot=(0,0), uUVScale=(1,1)
// and uUVOffset=(0,0) for every material, which collapses the sprite quad's
// transform chain to identity -- so a sprite shader still positions text
// correctly, it just samples the atlas as colour instead of decoding it.

#pragma vertex
#version 450 core
layout (location = 0) in vec2 aPos;   // glyph corner, text-local world units
layout (location = 1) in vec2 aUV;    // atlas UV

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vTexCoord;
// Emitted unconditionally even though this shader is unlit: they cost two
// varyings and they are exactly what a copy of this file needs in order to paste
// in the shared LIGHTING BLOCK from sprite.glsl and get lit signage, without any
// engine change.
out vec2 vWorldPos;
out mat2 vTBN;

void main() {
    vTexCoord = aUV;

    vec4 world = uModel * vec4(aPos, 0.0, 1.0);
    vWorldPos = world.xy;
    vTBN = mat2(normalize(uModel[0].xy), normalize(uModel[1].xy));

    gl_Position = uProj * uView * world;
}

#pragma fragment
#version 450 core
out vec4 FragColor;

in vec2 vTexCoord;
in vec2 vWorldPos;
in mat2 vTBN;

// Engine-bound, never material properties -- see EngineReservedUniforms in
// Material.cpp. uMSDF lives at the reserved texture slot so a material that
// gains another sampler can never stomp it; uPxRange is a property of the atlas
// bake, not of any material.
uniform sampler2D uMSDF;
uniform float     uPxRange = 4.0;

// Component-owned: pushed by TextRenderer::OverrideUniforms from its inspector
// fields, and also reserved so they don't appear as dead material controls.
uniform vec4  uColor        = vec4(1.0);
uniform float uWeight       = 0.0;   // -0.25 .. 0.25, thin .. bold
uniform vec4  uOutlineColor = vec4(0.0, 0.0, 0.0, 1.0);
uniform float uOutlineWidth = 0.0;   // 0 .. 0.35, in distance-field units

// The three channels each carry a distance to a different subset of the glyph's
// edges. Their median reconstructs sharp corners that a single-channel SDF
// rounds off -- this is the whole point of "multi-channel" SDF.
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

// How many screen pixels the atlas's distance range spans AT THIS FRAGMENT.
// Derived per-fragment via fwidth rather than from a uniform, which is what
// makes the antialiasing automatically correct under editor pan/zoom, any camera
// orthoSize, and any object scale -- nothing has to be recomputed when the view
// changes. Clamped to 1 so heavy minification degrades to a hard edge instead of
// a fully transparent smear.
float screenPxRange() {
    vec2 unitRange     = vec2(uPxRange) / vec2(textureSize(uMSDF, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(vTexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3  msd   = texture(uMSDF, vTexCoord).rgb;
    float sd    = median(msd.r, msd.g, msd.b);
    float range = screenPxRange();

    // 0.5 is the glyph edge in a signed distance field; shifting the threshold
    // by uWeight dilates or erodes the shape, which is a free faux-bold.
    float fillA = clamp(range * (sd - (0.5 - uWeight)) + 0.5, 0.0, 1.0);
    vec4  fill  = vec4(uColor.rgb, uColor.a * fillA);

    if (uOutlineWidth > 0.0) {
        // The outline is the same glyph at a lower threshold, i.e. dilated
        // further out, with the fill composited over it.
        float strokeA = clamp(range * (sd - (0.5 - uWeight - uOutlineWidth)) + 0.5, 0.0, 1.0);
        vec4  stroke  = vec4(uOutlineColor.rgb, uOutlineColor.a * strokeA);

        // Straight-alpha "fill over stroke". Doing this by hand rather than
        // leaning on the blend state keeps the pair a single correctly-composited
        // fragment, so a semi-transparent outline under a semi-transparent fill
        // doesn't double-darken where they overlap.
        float a = fill.a + stroke.a * (1.0 - fill.a);
        if (a < 0.001) discard;
        vec3 rgb = (fill.rgb * fill.a + stroke.rgb * stroke.a * (1.0 - fill.a)) / a;
        FragColor = vec4(rgb, a);
    } else {
        if (fill.a < 0.001) discard;
        FragColor = fill;
    }
}

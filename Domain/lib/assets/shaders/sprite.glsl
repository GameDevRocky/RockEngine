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
out vec2 vWorldPos;   // lighting works in world space, so the lights don't have
                      // to be transformed into each sprite's local frame
out mat2 vTBN;        // sprite -> world rotation for the normal map

void main() {
    // Compute per-vertex UVs
    vTexCoord = (aUV * uUVScale) + uUVOffset;

    // Scale quad by size
    vec2 scaledPos = aPos * uSize;

    // Apply pivot offset
    scaledPos -= uSize * uPivot;

    // Transform to world space
    vec4 world = uModel * vec4(scaledPos, 0.0, 1.0);
    vWorldPos = world.xy;

    // Rotate tangent-space normals with the sprite. Normalizing drops the model
    // matrix's scale, which must not skew the normal direction.
    vTBN = mat2(normalize(uModel[0].xy), normalize(uModel[1].xy));

    gl_Position = uProj * uView * world;
}

#pragma fragment
#version 450 core
out vec4 FragColor;

in vec2 vTexCoord;
in vec2 vWorldPos;
in mat2 vTBN;

uniform sampler2D uTexture;  // bound per sprite
uniform vec4 uColor = vec4(1.0); // per-sprite color multiplier
uniform float uTime;

// ═════════════════════════════════════════════════════════════════════════════
// LIGHTING BLOCK -- copy this whole section into any shader that should be lit.
//
// Nothing here needs wiring up per draw call: LightingPass keeps LightBlock
// bound at binding 0 and the shadow atlas bound at its reserved texture unit,
// so a shader is lit purely by declaring the block. Shaders that omit it
// (water, glow, glitch, outline) stay unlit by design.
//
// The array size and binding MUST match LightLimits in LightBuffer.hpp.
// ═════════════════════════════════════════════════════════════════════════════
const int MAX_LIGHTS = 32;

const int LIGHT_POINT       = 0;
const int LIGHT_SPOT        = 1;
const int LIGHT_DIRECTIONAL = 2;
// Type 3 (Global) never reaches the array -- LightingPass folds it into uAmbient.

struct GpuLight {
    vec4 posRange;   // xy = world pos (or direction if Directional) | z = range | w = type
    vec4 color;      // rgb = colour | a = intensity
    vec4 spot;       // xy = cone direction | z = cos(inner) | w = cos(outer)
    vec4 params;     // x = innerRadius | y = falloff | z = shadow row (-1 = none) | w = shadowStrength
    vec4 params2;    // x = height above the plane | y = normal influence | zw = reserved
};

layout(std140, binding = 0) uniform LightBlock {
    vec4     uAmbient;     // rgb = ambient colour | a = ambient intensity
    ivec4    uLightMeta;   // x = active light count
    GpuLight uLights[MAX_LIGHTS];
};

// Engine-bound, never a material property (see TextureSlots in EngineUtils.hpp
// and the reserved-name skip list in Material::Validate).
uniform sampler2D uNormalMap;
uniform float     uHasNormalMap = 0.0;
uniform sampler2D uShadowAtlas;

// Per-material: 0 = fully unlit (ignores every light), 1 = fully lit. Declared
// with a GLSL default so Material::Validate auto-seeds it and it shows up as a
// slider on every material using this shader.
uniform float uLitAmount = 1.0;

const int   SHADOW_RES  = 256;    // angular samples per quadrant; = ShadowAtlas::kResolution
const int   SHADOW_ROWS = 16;     // = LightLimits::kMaxShadowLights
const float SHADOW_BIAS = 0.004;  // normalized distance. Raise if a caster shadows itself.

// ─── SHARED WITH shadow_map.glsl ─────────────────────────────────────────────
// Rotates a light-relative offset so the quadrant's forward axis becomes .x.
// shadow_map.glsl builds the atlas with the identical mapping. If these two ever
// disagree, shadows come out rotated by 90 degrees at the quadrant seams --
// keep the two copies byte-identical.
vec2 RotateToQuadrant(vec2 d, int q) {
    if (q == 0) return vec2( d.x,  d.y);
    if (q == 1) return vec2( d.y, -d.x);
    if (q == 2) return vec2(-d.x, -d.y);
    return              vec2(-d.y,  d.x);
}
// ─────────────────────────────────────────────────────────────────────────────

// Which of the four 90-degree faces `d` points into. Must mirror the order
// RotateToQuadrant expects: +X, +Y, -X, -Y.
int QuadrantOf(vec2 d) {
    if (abs(d.x) >= abs(d.y)) return d.x >= 0.0 ? 0 : 2;
    return d.y >= 0.0 ? 1 : 3;
}

// 1.0 = fully lit, 0.0 = fully occluded.
float SampleShadow(int row, vec2 fragPos, vec2 lightPos, float range, float strength) {
    vec2 d = fragPos - lightPos;
    float dist = length(d) / range;

    int q = QuadrantOf(d);
    vec2 dq = RotateToQuadrant(d, q);
    if (dq.x <= 0.0) return 1.0;   // degenerate; nothing was rasterized here

    float u = (dq.y / dq.x) * 0.5 + 0.5;
    // Held off the quadrant edges: a tap that spills into the neighbouring
    // quadrant would read an unrelated direction's occluder distance.
    float texel = 1.0 / float(SHADOW_RES);
    u = clamp(u, texel * 0.5, 1.0 - texel * 0.5);

    float v = (float(row) + 0.5) / float(SHADOW_ROWS);
    float quadrantBase = float(q) * 0.25;

    // 3-tap PCF across the angular axis only -- the atlas has no second axis to
    // filter. Softens the stair-stepping the 256-sample resolution would show on
    // a long shadow edge.
    float lit = 0.0;
    for (int i = -1; i <= 1; ++i) {
        float su = clamp(u + float(i) * texel, texel * 0.5, 1.0 - texel * 0.5);
        float occluder = texture(uShadowAtlas, vec2(quadrantBase + su * 0.25, v)).r;
        lit += (dist - SHADOW_BIAS > occluder) ? 0.0 : 1.0;
    }
    lit /= 3.0;

    return mix(1.0, lit, strength);
}

vec3 ComputeLighting(vec3 N) {
    vec3 total = uAmbient.rgb * uAmbient.a;

    for (int i = 0; i < uLightMeta.x; ++i) {
        GpuLight L = uLights[i];
        int   type      = int(L.posRange.w);
        float range     = L.posRange.z;
        float intensity = L.color.a;

        vec3  toLight3;      // fragment -> light, with the light's height as Z
        float attenuation;

        if (type == LIGHT_DIRECTIONAL) {
            // No position and no falloff: a constant direction across the scene.
            // The height still matters -- it is what tilts the light out of the
            // sprite plane so a normal map has something to shade against.
            toLight3 = normalize(vec3(-L.posRange.xy, max(L.params2.x, 0.0001)));
            attenuation = 1.0;
        } else {
            vec2  toLight = L.posRange.xy - vWorldPos;
            float d       = length(toLight) / range;
            if (d > 1.0) continue;

            // innerRadius holds a full-brightness core, falloff shapes the ramp.
            attenuation = pow(1.0 - smoothstep(L.params.x, 1.0, d), L.params.y);

            if (type == LIGHT_SPOT) {
                vec2 fromLight = -toLight;
                float lenSq = dot(fromLight, fromLight);
                // At the exact centre there is no direction to test; treat it as
                // inside the cone rather than dividing by zero.
                if (lenSq > 1e-6) {
                    float cosAngle = dot(normalize(fromLight), L.spot.xy);
                    attenuation *= smoothstep(L.spot.w, L.spot.z, cosAngle);
                }
            }

            if (attenuation <= 0.0) continue;

            int shadowRow = int(L.params.z);
            if (shadowRow >= 0) {
                attenuation *= SampleShadow(shadowRow, vWorldPos, L.posRange.xy, range, L.params.w);
                if (attenuation <= 0.0) continue;
            }

            toLight3 = normalize(vec3(toLight, max(L.params2.x, 0.0001)));
        }

        // Normal influence of 0 gives flat lighting, which is what an unmapped
        // sprite wants -- N is (0,0,1) there and N.L would otherwise darken
        // everything that isn't directly beneath the light.
        float ndl = mix(1.0, max(dot(N, toLight3), 0.0), L.params2.y);

        total += L.color.rgb * (intensity * attenuation * ndl);
    }

    return total;
}
// ═════════════════════════ END LIGHTING BLOCK ════════════════════════════════

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);

    if (texColor.a < 0.01)
        discard;

    vec3 N = vec3(0.0, 0.0, 1.0);
    if (uHasNormalMap > 0.5) {
        vec3 n = texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0;
        N = normalize(vec3(vTBN * n.xy, max(n.z, 0.0001)));
    }

    // uLitAmount 0 reproduces the pre-lighting output exactly, and so does a
    // scene with no lights at all (LightingPass uploads white ambient + count 0).
    vec3 lighting = mix(vec3(1.0), ComputeLighting(N), uLitAmount);

    FragColor = vec4(texColor.rgb * uColor.rgb * lighting, texColor.a * uColor.a);
}

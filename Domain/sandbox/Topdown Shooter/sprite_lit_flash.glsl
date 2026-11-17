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
out vec2 vWorldPos;
out mat2 vTBN;

void main() {
    vTexCoord = (aUV * uUVScale) + uUVOffset;
    vec2 scaledPos = aPos * uSize;
    scaledPos -= uSize * uPivot;
    vec4 world = uModel * vec4(scaledPos, 0.0, 1.0);
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

uniform sampler2D uTexture;
uniform vec4 uColor = vec4(1.0);
uniform float uTime;

// Flash uniforms
uniform float uFlashAmount = 0.0;  // 0-1, where 1 is full white flash
uniform vec3 uFlashColor = vec3(1.0, 1.0, 1.0);  // Flash color (default white)

// ═════════════════════════════════════════════════════════════════════════════
// LIGHTING BLOCK
// ═════════════════════════════════════════════════════════════════════════════
const int MAX_LIGHTS = 32;

const int LIGHT_POINT       = 0;
const int LIGHT_SPOT        = 1;
const int LIGHT_DIRECTIONAL = 2;

struct GpuLight {
    vec4 posRange;
    vec4 color;
    vec4 spot;
    vec4 params;
    vec4 params2;
};

layout(std140, binding = 0) uniform LightBlock {
    vec4     uAmbient;
    ivec4    uLightMeta;
    GpuLight uLights[MAX_LIGHTS];
};

uniform sampler2D uNormalMap;
uniform float     uHasNormalMap = 0.0;
uniform sampler2D uShadowAtlas;
uniform float uLitAmount = 1.0;

const int   SHADOW_RES  = 256;
const int   SHADOW_ROWS = 16;
const float SHADOW_BIAS = 0.004;

vec2 RotateToQuadrant(vec2 d, int q) {
    if (q == 0) return vec2( d.x,  d.y);
    if (q == 1) return vec2( d.y, -d.x);
    if (q == 2) return vec2(-d.x, -d.y);
    return              vec2(-d.y,  d.x);
}

int QuadrantOf(vec2 d) {
    if (abs(d.x) >= abs(d.y)) return d.x >= 0.0 ? 0 : 2;
    return d.y >= 0.0 ? 1 : 3;
}

float SampleShadow(int row, vec2 fragPos, vec2 lightPos, float range, float strength) {
    vec2 d = fragPos - lightPos;
    float dist = length(d) / range;

    int q = QuadrantOf(d);
    vec2 dq = RotateToQuadrant(d, q);
    if (dq.x <= 0.0) return 1.0;

    float u = (dq.y / dq.x) * 0.5 + 0.5;
    float texel = 1.0 / float(SHADOW_RES);
    u = clamp(u, texel * 0.5, 1.0 - texel * 0.5);

    float v = (float(row) + 0.5) / float(SHADOW_ROWS);
    float quadrantBase = float(q) * 0.25;

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

        vec3  toLight3;
        float attenuation;

        if (type == LIGHT_DIRECTIONAL) {
            toLight3 = normalize(vec3(-L.posRange.xy, max(L.params2.x, 0.0001)));
            attenuation = 1.0;
        } else {
            vec2  toLight = L.posRange.xy - vWorldPos;
            float d       = length(toLight) / range;
            if (d > 1.0) continue;

            attenuation = pow(1.0 - smoothstep(L.params.x, 1.0, d), L.params.y);

            if (type == LIGHT_SPOT) {
                vec2 fromLight = -toLight;
                float lenSq = dot(fromLight, fromLight);
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

    vec3 lighting = mix(vec3(1.0), ComputeLighting(N), uLitAmount);

    // Apply base color and lighting
    vec3 finalColor = texColor.rgb * uColor.rgb * lighting;

    // Apply flash effect - lerp towards flash color based on flash amount
    finalColor = mix(finalColor, uFlashColor, uFlashAmount);

    FragColor = vec4(finalColor, texColor.a * uColor.a);
}

#pragma domain text

// Lit MSDF text. The glyph mesh is already positioned in text-local world
// units and carries final atlas UVs, so it uses the same vertex contract as the
// default unlit msdf_text shader.
#pragma vertex
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vTexCoord;
out vec2 vWorldPos;

void main() {
    vTexCoord = aUV;
    vec4 world = uModel * vec4(aPos, 0.0, 1.0);
    vWorldPos = world.xy;
    gl_Position = uProj * uView * world;
}

#pragma fragment
#version 450 core
out vec4 FragColor;

in vec2 vTexCoord;
in vec2 vWorldPos;

// Font/TextRenderer-owned uniforms. Material::Validate reserves these for the
// text domain so they do not appear as material controls.
uniform sampler2D uMSDF;
uniform float     uPxRange      = 4.0;
uniform vec4      uColor        = vec4(1.0);
uniform float     uWeight       = 0.0;
uniform vec4      uOutlineColor = vec4(0.0, 0.0, 0.0, 1.0);
uniform float     uOutlineWidth = 0.0;

// Shared lighting contract. Keep this layout synchronized with LightBuffer.hpp
// and sprite.glsl.
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

uniform sampler2D uShadowAtlas;

// Allows a lit text material to fade continuously back to unlit while keeping
// the default text material on the separate unlit shader.
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

    float texel = 1.0 / float(SHADOW_RES);
    float u = clamp((dq.y / dq.x) * 0.5 + 0.5,
                    texel * 0.5, 1.0 - texel * 0.5);
    float v = (float(row) + 0.5) / float(SHADOW_ROWS);
    float quadrantBase = float(q) * 0.25;

    float lit = 0.0;
    for (int i = -1; i <= 1; ++i) {
        float su = clamp(u + float(i) * texel,
                         texel * 0.5, 1.0 - texel * 0.5);
        float occluder = texture(
            uShadowAtlas, vec2(quadrantBase + su * 0.25, v)).r;
        lit += (dist - SHADOW_BIAS > occluder) ? 0.0 : 1.0;
    }

    return mix(1.0, lit / 3.0, strength);
}

vec3 ComputeLighting() {
    vec3 total = uAmbient.rgb * uAmbient.a;

    for (int i = 0; i < uLightMeta.x; ++i) {
        GpuLight light = uLights[i];
        int type = int(light.posRange.w);
        float range = light.posRange.z;
        float attenuation;
        vec3 toLight3;

        if (type == LIGHT_DIRECTIONAL) {
            toLight3 = normalize(vec3(
                -light.posRange.xy, max(light.params2.x, 0.0001)));
            attenuation = 1.0;
        } else {
            vec2 toLight = light.posRange.xy - vWorldPos;
            float d = length(toLight) / range;
            if (d > 1.0) continue;

            attenuation = pow(
                1.0 - smoothstep(light.params.x, 1.0, d), light.params.y);

            if (type == LIGHT_SPOT) {
                vec2 fromLight = -toLight;
                float lenSq = dot(fromLight, fromLight);
                if (lenSq > 1e-6) {
                    float cosAngle = dot(normalize(fromLight), light.spot.xy);
                    attenuation *= smoothstep(
                        light.spot.w, light.spot.z, cosAngle);
                }
            }

            if (attenuation <= 0.0) continue;

            int shadowRow = int(light.params.z);
            if (shadowRow >= 0) {
                attenuation *= SampleShadow(
                    shadowRow, vWorldPos, light.posRange.xy,
                    range, light.params.w);
                if (attenuation <= 0.0) continue;
            }

            toLight3 = normalize(vec3(
                toLight, max(light.params2.x, 0.0001)));
        }

        // Text has no normal map, so its surface normal faces out of the 2D
        // plane. normalInfluence still behaves exactly as it does for an
        // unmapped sprite: zero gives flat light, one applies N dot L.
        float ndl = mix(1.0, max(toLight3.z, 0.0), light.params2.y);
        total += light.color.rgb * (light.color.a * attenuation * ndl);
    }

    return total;
}

float Median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange() {
    vec2 unitRange = vec2(uPxRange) / vec2(textureSize(uMSDF, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(vTexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3 msd = texture(uMSDF, vTexCoord).rgb;
    float sd = Median(msd.r, msd.g, msd.b);
    float range = ScreenPxRange();

    float fillA = clamp(
        range * (sd - (0.5 - uWeight)) + 0.5, 0.0, 1.0);
    vec4 fill = vec4(uColor.rgb, uColor.a * fillA);
    vec4 glyph = fill;

    if (uOutlineWidth > 0.0) {
        float strokeA = clamp(
            range * (sd - (0.5 - uWeight - uOutlineWidth)) + 0.5,
            0.0, 1.0);
        vec4 stroke = vec4(
            uOutlineColor.rgb, uOutlineColor.a * strokeA);

        float alpha = fill.a + stroke.a * (1.0 - fill.a);
        if (alpha < 0.001) discard;
        vec3 rgb = (fill.rgb * fill.a
                  + stroke.rgb * stroke.a * (1.0 - fill.a)) / alpha;
        glyph = vec4(rgb, alpha);
    } else if (fill.a < 0.001) {
        discard;
    }

    vec3 lighting = mix(vec3(1.0), ComputeLighting(), uLitAmount);
    FragColor = vec4(glyph.rgb * lighting, glyph.a);
}

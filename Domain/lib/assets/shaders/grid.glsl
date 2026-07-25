#pragma vertex
#version 450 core

layout(location = 0) in vec3 aPos;
out vec2 WorldPos;

uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    vec4 clip = vec4(aPos.xy, 0.0, 1.0);
    vec4 world = inverse(uProj * uView) * clip;

    WorldPos = world.xy / world.w;
    gl_Position = clip;
}

#pragma fragment
#version 450 core

in vec2 WorldPos;
out vec4 FragColor;

uniform float uPixelsPerWorldUnit;  // True screen pixels per world unit at current zoom
uniform float uBaseSpacing;         // Base (major) cell size in world units == GridCellPixels
uniform float uTime;

// Levels finer (negative k) and coarser (positive k) than the base cell. The base
// cell is level 0; every MAJOR_SKIP-th level from it is a "major" line.
const int K_MIN = -4;
const int K_MAX =  8;
const int MAJOR_SKIP = 4;
const vec3 uMinorColor = vec3(0.25f);
const vec3 uMajorColor = vec3(0.75f);

// A level fades IN as its on-screen cell size grows past FADE_MIN..FADE_FULL px.
// There is deliberately NO high-end fade-out: coarse levels persist as sparse
// major lines. Zooming in therefore reveals finer subdivisions while coarser
// lines remain -- the base cell reads as ~uBaseSpacing px at zoom 1.
const float FADE_MIN_PX  = 32.0;   // lines stay hidden until cells are a bit larger
const float FADE_FULL_PX = 64.0;   // wider band -> a longer, more visible fade ramp
// Nonlinear falloff (>1): partially-faded levels are pushed dimmer, so a fine grid
// dissolves harder as you zoom out instead of lingering as a faint smear.
const float FADE_POWER   = 2.2;


float distanceToNearestLine(float p, float spacing)
{
    float wrapped = mod(p + 0.5 * spacing, spacing);
    float centered = wrapped - 0.5 * spacing;
    return abs(centered);
}


void main()
{
    vec2 dd = fwidth(WorldPos);

    float minorIntensity = 0.0;
    float majorIntensity = 0.0;

    for (int k = K_MIN; k <= K_MAX; k++)
    {
        float spacing = uBaseSpacing * exp2(float(k));
        float pixelsPerCell = spacing * uPixelsPerWorldUnit;

        float fade = pow(smoothstep(FADE_MIN_PX, FADE_FULL_PX, pixelsPerCell), FADE_POWER);
        if (fade < 0.001) continue;

        float distX = distanceToNearestLine(WorldPos.x, spacing);
        float distY = distanceToNearestLine(WorldPos.y, spacing);
        float dist = min(distX, distY);

        float lineAA = max(dd.x, dd.y);
        float intensity = 1.0 - smoothstep(0.0, lineAA, dist);

        float currentIntensity = intensity * fade;

        // Anchor "major" to the base cell (k == 0) regardless of K_MIN's parity.
        bool isMajor = (k >= 0) ? (k % MAJOR_SKIP == 0)
                                : ((-k) % MAJOR_SKIP == 0);

        if (isMajor) {
            majorIntensity = max(majorIntensity, currentIntensity);
        } else {
            minorIntensity = max(minorIntensity, currentIntensity);
        }
    }
    vec3 finalRGB = mix(uMinorColor, uMajorColor, majorIntensity);
    float alpha = max(majorIntensity, minorIntensity);
    FragColor = vec4(finalRGB, alpha);
    if (alpha < 0.0001) discard;
}

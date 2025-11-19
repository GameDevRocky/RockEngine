#version 450 core
out vec4 FragColor;

uniform mat4 uView;
uniform mat4 uProj;
uniform float uZoom;
uniform vec2 uViewportSize;

vec2 WorldPos()
{
    vec2 frag = gl_FragCoord.xy;
    vec2 ndc = (frag / uViewportSize) * 2.0 - 1.0;
    ndc.y *= -1.0;

    vec4 clip = vec4(ndc, 0.0, 1.0);
    vec4 world = inverse(uProj * uView) * clip;
    return world.xy / world.w;
}

vec4 grid(vec2 worldPos)
{
    //-----------------------------------
    // Grid settings
    //-----------------------------------
    float cellSize = 1.0;      
    float lineWidthPx = 6.0;   // line thickness in pixels (thicker)
    vec3 lineColor = vec3(0.15); // dark grid lines

    //-----------------------------------
    // Pixel-scale derivative
    //-----------------------------------
    float dx = length(vec2(dFdx(worldPos.x), dFdy(worldPos.x)));
    float dy = length(vec2(dFdx(worldPos.y), dFdy(worldPos.y)));
    float pixelScale = max(dx, dy);

    //-----------------------------------
    // Distance to nearest grid line (world units)
    //-----------------------------------
    float distX = abs(mod(worldPos.x, cellSize));
    float distY = abs(mod(worldPos.y, cellSize));

    // normalize by pixel width → how close the fragment is to a line
    float dist = min(distX, distY) / (pixelScale * lineWidthPx);

    float line = 1.0 - clamp(dist, 0.0, 1.0);

    return vec4(lineColor, line);
}

void main()
{
    vec2 world = WorldPos();
    vec4 g = grid(world);

    vec3 bg = vec3(0.05); // dark background
    FragColor = vec4(mix(bg, g.rgb, g.a), 1.0);
}

#version 450 core

in vec2 WorldPos;
out vec4 FragColor;
// Grid settings
const float minorCell = 1.0;
const float majorCell = 4.0;
const float lineWidth = 0.025;      // minor line width
const float majorLineWidth = 0.05;  // major line width
const vec3 minorColor = vec3(0.15);
const vec3 majorColor = vec3(0.25);
const vec3 bgColor = vec3(0.05);

void main()
{
    // Distance to nearest minor line
    float distX = abs(mod(WorldPos.x, minorCell));
    float distY = abs(mod(WorldPos.y, minorCell));
    float minorDist = min(distX, distY);

    // Distance to nearest major line
    float majorDistX = abs(mod(WorldPos.x, majorCell));
    float majorDistY = abs(mod(WorldPos.y, majorCell));
    float majorDist = min(majorDistX, majorDistY);

    // Compute alpha for blending lines
    float minorAlpha = 1.0 - smoothstep(0.0, lineWidth, minorDist);
    float majorAlpha = 1.0 - smoothstep(0.0, majorLineWidth, majorDist);

    // Combine colors giving priority to major lines
    vec3 color = mix(minorColor, majorColor, majorAlpha);
    color = mix(bgColor, color, max(minorAlpha, majorAlpha));

    FragColor = vec4(color, 1.0);
}

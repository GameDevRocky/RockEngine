#version 450 core

in vec2 WorldPos;
out vec4 FragColor;

uniform float time;

// Grid settings
const float cellSize = 1.0;
const float lineWidth = 0.05;  // in world units
const vec3 lineColor = vec3(0.15);
const vec3 bgColor = vec3(0.05);

void main()
{
    // Compute distance to nearest grid line
    float distX = abs(mod(WorldPos.x, cellSize));
    float distY = abs(mod(WorldPos.y, cellSize));
    float dist = min(distX, distY);

    // Smooth line blending
    float alpha = 1.0 - smoothstep(0.0, lineWidth, dist);

    vec3 color = mix(bgColor, lineColor, alpha);
    FragColor = vec4(color, 1.0);
}

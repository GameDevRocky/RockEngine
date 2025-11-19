#version 450 core

layout(location = 0) in vec3 aPos; // fullscreen triangle in NDC [-1,1]
out vec2 WorldPos;

uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    vec4 clipPos = vec4(aPos.xy, 0.0, 1.0);
    vec4 world = inverse(uProj * uView) * clipPos;
    WorldPos = world.xy / world.w;  // pass world position to fragment
    gl_Position = clipPos;          // keep NDC position
}

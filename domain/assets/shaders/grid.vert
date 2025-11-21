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

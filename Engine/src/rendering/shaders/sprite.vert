#version 450 core

layout (location = 0) in vec3 aPos;      // Quad vertex position
layout (location = 1) in vec2 aTexCoord; // Quad UV

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aTexCoord;
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}

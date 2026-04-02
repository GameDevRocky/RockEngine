#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2 uSize;
uniform vec2 uPivot;
uniform vec2 uUVScale;
uniform vec2 uUVOffset;

void main()
{
    // Scale quad by size
    vec2 scaledPos = aPos * uSize;
    
    // Apply pivot offset (same as sprite shader)
    scaledPos -= uSize * uPivot;
    
    gl_Position = uProj * uView * uModel * vec4(scaledPos, 0.0, 1.0);
    vTexCoord = aTexCoord * uUVScale + uUVOffset;
}

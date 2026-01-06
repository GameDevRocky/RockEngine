#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView; 
uniform mat4 uProj;

uniform vec2 uUVScale = vec2(1.0, 1.0);  
uniform vec2 uUVOffset = vec2(0.0, 0.0);

out vec2 vTexCoord;

void main() {
    vTexCoord = (aUV * uUVScale) + uUVOffset;
    gl_Position = uProj * uView * uModel * vec4(aPos, 0.0, 1.0);
}
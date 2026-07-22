#pragma vertex
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aSemi;

struct InstanceData {
    mat4 model;
    vec2 size;
    vec2 semiSize;
    vec2 offset;
    vec2 pivot;
    vec4 color;
};

layout (std430, binding = 0) buffer InstanceBuffer {
    InstanceData instances[];
};

uniform mat4 uView;
uniform mat4 uProj;

flat out vec4 vColor;

void main() {
    InstanceData inst = instances[gl_InstanceID];

    vec2 scaledPos = aPos * inst.size + aSemi * inst.semiSize;
    scaledPos -= inst.size * inst.pivot;
    scaledPos += inst.offset;

    gl_Position = uProj * uView * inst.model * vec4(scaledPos, 0.0, 1.0);
    vColor = inst.color;
}

#pragma fragment
#version 450 core
out vec4 FragColor;

flat in vec4 vColor;

void main()
{
    FragColor = vColor;
}

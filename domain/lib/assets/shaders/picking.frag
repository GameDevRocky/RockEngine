#version 450 core
layout(location = 0) out uint FragID;

in vec2 vTexCoord;

uniform int uId;
uniform sampler2D uTexture;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    if(texColor.a < 0.1) discard;
    FragID = uint(uId);
}

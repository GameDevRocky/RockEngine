#version 450 core
out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform vec4 uColor = vec4(1.0);
uniform float uTime;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.01)
        discard;

    FragColor = texColor * uColor;
    //FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
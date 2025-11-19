#version 450 core

out vec4 FragColor;

in vec2 vTexCoord;


uniform sampler2D uTexture;
uniform vec4 uColor;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);

    // Alpha discard optional
    if (texColor.a <= 0.01)
        discard;

    FragColor = texColor * uColor;
}

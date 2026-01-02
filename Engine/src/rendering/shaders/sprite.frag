#version 450 core

out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uTexture;
uniform vec4 uTint; // sprite color tint (optional, can be 1,1,1,1)

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);

    // Alpha discard optional
    if (texColor.a <= 0.01)
        discard;

    FragColor = texColor * uTint;
}

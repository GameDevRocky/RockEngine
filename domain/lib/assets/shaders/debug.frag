#version 450 core
out vec4 FragColor;

uniform vec4 uColor = vec4(1.0, 1.0, 1.0, 1.0); // default white

void main()
{
    FragColor = uColor;
}

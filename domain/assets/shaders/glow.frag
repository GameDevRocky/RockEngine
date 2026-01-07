#version 450 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uTime;

#define EdgeColor vec3(0.2, 0.2, 0.15)
uniform vec4 uColor; // Added to match your YAML and C++
void main()
{
    vec4 tex = texture(uTexture, vTexCoord);
    if (tex.a <= 0.001)
    {
        FragColor = vec4(0.0);
        return;
    }

    vec3 color = tex.rgb;
    float alpha = tex.a;
    float edge = mix(1.0, tex.r, alpha);
    float luminance = dot(color, vec3(0.299, 0.587, 0.114));
    float pulse = sin(uTime * 1.5) * 0.05;
    float threshold = 0.35 + pulse;

    float w = fwidth(luminance) * 2.0;
    float toonLight = smoothstep(-w, w, luminance - threshold);

    // Shade original color
    vec3 shadedColor = mix(color * 0.6, color, toonLight);

    // Apply ink
    vec3 finalRGB = mix(EdgeColor, shadedColor, edge);

    // 🔑 CRITICAL LINE 🔑
    finalRGB *= alpha;

    FragColor = vec4(finalRGB, alpha);
}

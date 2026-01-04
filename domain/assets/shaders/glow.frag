#version 450 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uTime;

#define EdgeColor vec3(0.2, 0.2, 0.15)

void main()
{
    vec4 tex = texture(uTexture, vTexCoord);

    // Preserve transparency early
    if (tex.a <= 0.001)
    {
        FragColor = vec4(0.0);
        return;
    }

    vec3 color = tex.rgb;
    float alpha = tex.a;

    // --- IMPORTANT ---
    // Do NOT trust edge mask where alpha is low
    float edge = mix(1.0, tex.r, alpha);

    // Luminance from color
    float luminance = dot(color, vec3(0.299, 0.587, 0.114));

    // Subtle toon animation
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

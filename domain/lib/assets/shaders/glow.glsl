#pragma vertex
#version 450 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2 uSize = vec2(1.0, 1.0);
uniform vec2 uPivot = vec2(0.5, 0.5);
uniform vec2 uUVScale = vec2(1.0, 1.0);
uniform vec2 uUVOffset = vec2(0.0, 0.0);

out vec2 vTexCoord;

void main() {
    vTexCoord = (aUV * uUVScale) + uUVOffset;
    vec2 scaledPos = aPos * uSize;
    scaledPos -= uSize * uPivot;
    gl_Position = uProj * uView * uModel * vec4(scaledPos, 0.0, 1.0);
}

#pragma fragment
#version 450 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uTime;

#define EdgeColor vec3(0.2, 0.2, 0.15)
uniform vec4 uColor = vec4(1.0, 1.0, 1.0, 1.0);

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

    finalRGB *= alpha;

    FragColor = vec4(finalRGB, alpha);
}

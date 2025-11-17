#version 450 core
    out vec4 FragColor;

    uniform mat4 uView;
    uniform mat4 uProj;
    uniform float uZoom;
    uniform vec2 uViewportSize;
    uniform float time;

    vec2 WorldPos()
    {
        vec2 fragPos = gl_FragCoord.xy;
        vec2 ndc = (fragPos / uViewportSize) * 2.0 - 1.0;
        ndc.y *= -1.0;
        vec4 clip = vec4(ndc, 0.0, 1.0);
        vec4 world = inverse(uProj * uView) * clip;
        return world.xy / world.w;
    }

    float max2(vec2 v) {
        return max(v.x, v.y);
    }

    float log10(float x) {
        return log(x) / log(10.0);
    }

    vec4 grid(vec2 uv) {
        float minCellSize = 0.01;
        float minCellPixelWidth = 2.0;
        float lineWidth = 4.0;
        vec3 thinColor = vec3(0.5, 0.5, 0.5);
        vec3 thickColor = vec3(0.0, 0.0, 0.0);

        vec2 dudv = vec2(
            length(vec2(dFdx(uv.x), dFdy(uv.x))),
            length(vec2(dFdx(uv.y), dFdy(uv.y)))
        );

        float lod = max(0.0, log10((max2(dudv) * minCellPixelWidth) / minCellSize) + 1.0);
        float fade = fract(lod);

        float lod0 = minCellSize * pow(10.0, floor(lod));
        float lod1 = lod0 * 10.0;
        float lod2 = lod1 * 10.0;

        float lod0a = max2(vec2(1.0) - abs(clamp(mod(uv, lod0) / dudv / lineWidth, 0.0, 1.0) * 2.0 - vec2(1.0)));
        float lod1a = max2(vec2(1.0) - abs(clamp(mod(uv, lod1) / dudv / lineWidth, 0.0, 1.0) * 2.0 - vec2(1.0)));
        float lod2a = max2(vec2(1.0) - abs(clamp(mod(uv, lod2) / dudv / lineWidth, 0.0, 1.0) * 2.0 - vec2(1.0)));

        return vec4(
            lod2a > 0.0 ? thickColor : lod1a > 0.0 ? mix(thickColor, thinColor, fade) : thinColor,
            lod2a > 0.0 ? lod2a : lod1a > 0.0 ? lod1a : lod0a * (1.0 - fade)
        );
    }

    void main()
    {
        vec2 worldPos = WorldPos();
        vec4 gridColor = grid(worldPos);
        FragColor = vec4(mix(vec3(0.05, 0.05, 0.05), gridColor.rgb, gridColor.a), 1.0);
    }
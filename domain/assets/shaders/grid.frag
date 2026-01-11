#version 450 core

in vec2 WorldPos;
out vec4 FragColor;

uniform float uZoom;      
uniform float uTime;      
const int MAX_LEVELS = 13; 

const float BASE_SPACING = .5f; 
const int MAJOR_SKIP = 2;        
const vec3 uMinorColor = vec3(0.2f);
const vec3 uMajorColor = vec3(0.5f);
const vec3 uBackground = vec3(0.01f); 

const float FADE_IN_PIXELS  = 6.0;   
const float HIDE_PIXELS     = 500.0; 
const float FADE_OUT_PIXELS = 300.0; 


float distanceToNearestLine(float p, float spacing)
{
 
    float wrapped = mod(p + 0.5 * spacing, spacing); 
    
    float centered = wrapped - 0.5 * spacing;
    
    return abs(centered);
}


void main()
{
    vec2 dd = fwidth(WorldPos); 

    float minorIntensity = 0.0; 
    float majorIntensity = 0.0; 

    for (int i = 0; i < MAX_LEVELS; i++)
    {
        float spacing = BASE_SPACING * pow(2.0, float(i));
        float pixelsPerCell = spacing * uZoom;

        if (pixelsPerCell > HIDE_PIXELS) break;

        float fadeIn  = smoothstep(1.0, FADE_IN_PIXELS, pixelsPerCell);
        float fadeOut = 1.0 - smoothstep(FADE_OUT_PIXELS, HIDE_PIXELS, pixelsPerCell);
        float fade = fadeIn * fadeOut;

        if (fade < 0.001) continue;

        float distX = distanceToNearestLine(WorldPos.x, spacing);
        float distY = distanceToNearestLine(WorldPos.y, spacing);
        float dist = min(distX, distY); 

        float lineAA = max(dd.x, dd.y) * 1.0; 
        float intensity = 1.0 - smoothstep(0.0, lineAA, dist);

        float currentIntensity = intensity * fade;
        bool isMajor = (i % MAJOR_SKIP) == 0;

        if (isMajor) {
            majorIntensity = max(majorIntensity, currentIntensity);
        } else {
            minorIntensity = max(minorIntensity, currentIntensity);
        }
    }
    vec3 finalRGB = mix(uMinorColor, uMajorColor, majorIntensity);
    float alpha = max(majorIntensity, minorIntensity);
    FragColor = vec4(finalRGB, alpha);
    if (alpha < 0.0001) discard;
}
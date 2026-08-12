#pragma vertex
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2 uSize;
uniform vec2 uPivot;
uniform vec2 uUVScale;
uniform vec2 uUVOffset;

void main()
{
    // Scale quad by size
    vec2 scaledPos = aPos * uSize;
    
    // Apply pivot offset (same as sprite shader)
    scaledPos -= uSize * uPivot;
    
    gl_Position = uProj * uView * uModel * vec4(scaledPos, 0.0, 1.0);
    vTexCoord = aTexCoord * uUVScale + uUVOffset;
}

#pragma fragment
#version 450 core
layout(location = 0) out uint FragID;

in vec2 vTexCoord;

uniform int uId;
uniform sampler2D uTexture;

// Sprites are picked through their own alpha so a click passes through the
// transparent parts of the quad. Some renderables have no texture to test
// against -- a TextRenderer is picked as its measured block rectangle, and an
// unbound sampler reads as fully transparent, which would discard every fragment
// and make the object unclickable. Those set this to 0 to pick the whole quad.
uniform float uAlphaTest = 1.0;

void main()
{
    vec4 texColor = texture(uTexture, vTexCoord);
    if(uAlphaTest > 0.5 && texColor.a < 0.1) discard;
    FragID = uint(uId);
}

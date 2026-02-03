#version 450 core
layout (location = 0) in vec2 aPos; // [-0.5, 0.5] quad corners

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2 uSize = vec2(1.0, 1.0);   // sprite/collider width/height in world units
uniform vec2 uPivot = vec2(0.5, 0.5);  // pivot (0..1)
uniform vec2 uOffset = vec2(0.0, 0.0); // collider center offset

void main() {
    // Scale the debug outline by size
    vec2 scaledPos = aPos * uSize;
    
    // Apply pivot offset (same as sprite rendering)
    scaledPos -= uSize * uPivot;
    
    // Apply collider center offset (in local space)
    scaledPos += uOffset;
    
    // Transform to clip space
    gl_Position = uProj * uView * uModel * vec4(scaledPos, 0.0, 1.0);
}

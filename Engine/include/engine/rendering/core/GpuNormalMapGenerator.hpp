#pragma once

#include <glad/glad.h>

struct GpuNormalMapSettings {
    float strength = 2.0f;
    int blurRadius = 1;
    bool useAlpha = false;
    bool useScharr = false;
    bool invertX = false;
    bool invertY = false;
    bool repeat = false;
};

namespace GpuNormalMapGenerator {

// Generates an RGBA8 tangent-space normal texture from an existing albedo
// texture. The caller must have a current OpenGL 4.3+ context. `destination`
// is created when zero and reused otherwise. Returns false before dispatch if
// compute/image-load-store support or shader initialization is unavailable.
bool Generate(GLuint source, GLuint& destination, int width, int height,
              const GpuNormalMapSettings& settings);

} // namespace GpuNormalMapGenerator

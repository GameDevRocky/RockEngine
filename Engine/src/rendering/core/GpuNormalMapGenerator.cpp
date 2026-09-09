#include "engine/rendering/core/GpuNormalMapGenerator.hpp"

#include <algorithm>
#include <iostream>

namespace {

constexpr GLuint kLocalSize = 16;
constexpr GLuint kInputImageUnit = 6;
constexpr GLuint kOutputImageUnit = 7;

const char* kHorizontalShader = R"(#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D uSource;
layout(r32f, binding = 7) writeonly uniform image2D uOutput;

uniform ivec2 uSize;
uniform int uRadius;
uniform int uUseAlpha;
uniform int uRepeat;

int sampleCoord(int value, int size) {
    if (uRepeat != 0) {
        int wrapped = value % size;
        return wrapped < 0 ? wrapped + size : wrapped;
    }
    return clamp(value, 0, size - 1);
}

float sourceHeight(ivec2 p) {
    vec4 color = texelFetch(uSource, p, 0);
    return uUseAlpha != 0
        ? color.a
        : dot(color.rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, uSize))) return;

    float sum = 0.0;
    for (int offset = -uRadius; offset <= uRadius; ++offset) {
        ivec2 samplePixel = ivec2(sampleCoord(pixel.x + offset, uSize.x), pixel.y);
        sum += sourceHeight(samplePixel);
    }
    imageStore(uOutput, pixel, vec4(sum / float(uRadius * 2 + 1)));
}
)";

const char* kVerticalShader = R"(#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(r32f, binding = 6) readonly uniform image2D uInput;
layout(r32f, binding = 7) writeonly uniform image2D uOutput;

uniform ivec2 uSize;
uniform int uRadius;
uniform int uRepeat;

int sampleCoord(int value, int size) {
    if (uRepeat != 0) {
        int wrapped = value % size;
        return wrapped < 0 ? wrapped + size : wrapped;
    }
    return clamp(value, 0, size - 1);
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, uSize))) return;

    float sum = 0.0;
    for (int offset = -uRadius; offset <= uRadius; ++offset) {
        ivec2 samplePixel = ivec2(pixel.x, sampleCoord(pixel.y + offset, uSize.y));
        sum += imageLoad(uInput, samplePixel).r;
    }
    imageStore(uOutput, pixel, vec4(sum / float(uRadius * 2 + 1)));
}
)";

const char* kNormalShader = R"(#version 430 core
layout(local_size_x = 16, local_size_y = 16) in;

layout(r32f, binding = 6) readonly uniform image2D uHeight;
layout(rgba8, binding = 7) writeonly uniform image2D uNormal;

uniform ivec2 uSize;
uniform float uStrength;
uniform int uUseScharr;
uniform int uInvertX;
uniform int uInvertY;
uniform int uRepeat;

int sampleCoord(int value, int size) {
    if (uRepeat != 0) {
        int wrapped = value % size;
        return wrapped < 0 ? wrapped + size : wrapped;
    }
    return clamp(value, 0, size - 1);
}

float heightAt(ivec2 p) {
    ivec2 samplePixel = ivec2(sampleCoord(p.x, uSize.x), sampleCoord(p.y, uSize.y));
    return imageLoad(uHeight, samplePixel).r;
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(pixel, uSize))) return;

    float dx = 0.0;
    float dy = 0.0;
    if (uUseScharr != 0) {
        dx = (-3.0 * heightAt(pixel + ivec2(-1, -1))
              +3.0 * heightAt(pixel + ivec2( 1, -1))
             -10.0 * heightAt(pixel + ivec2(-1,  0))
             +10.0 * heightAt(pixel + ivec2( 1,  0))
              -3.0 * heightAt(pixel + ivec2(-1,  1))
              +3.0 * heightAt(pixel + ivec2( 1,  1))) / 32.0;
        dy = (-3.0 * heightAt(pixel + ivec2(-1, -1))
             -10.0 * heightAt(pixel + ivec2( 0, -1))
              -3.0 * heightAt(pixel + ivec2( 1, -1))
              +3.0 * heightAt(pixel + ivec2(-1,  1))
             +10.0 * heightAt(pixel + ivec2( 0,  1))
              +3.0 * heightAt(pixel + ivec2( 1,  1))) / 32.0;
    } else {
        dx = (-heightAt(pixel + ivec2(-1, -1))
              +heightAt(pixel + ivec2( 1, -1))
              -2.0 * heightAt(pixel + ivec2(-1,  0))
              +2.0 * heightAt(pixel + ivec2( 1,  0))
              -heightAt(pixel + ivec2(-1,  1))
              +heightAt(pixel + ivec2( 1,  1))) / 8.0;
        dy = (-heightAt(pixel + ivec2(-1, -1))
              -2.0 * heightAt(pixel + ivec2( 0, -1))
              -heightAt(pixel + ivec2( 1, -1))
              +heightAt(pixel + ivec2(-1,  1))
              +2.0 * heightAt(pixel + ivec2( 0,  1))
              +heightAt(pixel + ivec2( 1,  1))) / 8.0;
    }

    float signX = uInvertX != 0 ? -1.0 : 1.0;
    float signY = uInvertY != 0 ? -1.0 : 1.0;
    vec3 normal = normalize(vec3(-dx * uStrength * signX,
                                 -dy * uStrength * signY,
                                 1.0));
    imageStore(uNormal, pixel, vec4(normal * 0.5 + 0.5, 1.0));
}
)";

GLuint CompileComputeShader(const char* source, const char* passName)
{
    const GLuint shader = glad_glCreateShader(GL_COMPUTE_SHADER);
    glad_glShaderSource(shader, 1, &source, nullptr);
    glad_glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glad_glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;

    char log[4096]{};
    glad_glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::cerr << "GpuNormalMapGenerator: " << passName
              << " compute shader compilation failed:\n" << log << std::endl;
    glad_glDeleteShader(shader);
    return 0;
}

GLuint BuildProgram(const char* source, const char* passName)
{
    const GLuint shader = CompileComputeShader(source, passName);
    if (!shader) return 0;

    const GLuint program = glad_glCreateProgram();
    glad_glAttachShader(program, shader);
    glad_glLinkProgram(program);
    glad_glDeleteShader(shader);

    GLint linked = GL_FALSE;
    glad_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) return program;

    char log[4096]{};
    glad_glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::cerr << "GpuNormalMapGenerator: " << passName
              << " compute program linking failed:\n" << log << std::endl;
    glad_glDeleteProgram(program);
    return 0;
}

struct Programs {
    GLuint horizontal = 0;
    GLuint vertical = 0;
    GLuint normal = 0;
    bool attempted = false;

    bool EnsureInitialized()
    {
        if (attempted) return horizontal && vertical && normal;
        attempted = true;

        if (!glad_glDispatchCompute || !glad_glBindImageTexture ||
            !glad_glMemoryBarrier) return false;

        horizontal = BuildProgram(kHorizontalShader, "horizontal blur");
        vertical = BuildProgram(kVerticalShader, "vertical blur");
        normal = BuildProgram(kNormalShader, "normal encoding");
        if (horizontal && vertical && normal) return true;

        if (horizontal) glad_glDeleteProgram(horizontal);
        if (vertical) glad_glDeleteProgram(vertical);
        if (normal) glad_glDeleteProgram(normal);
        horizontal = vertical = normal = 0;
        return false;
    }
};

Programs& GetPrograms()
{
    static Programs programs;
    return programs;
}

void SetInt(GLuint program, const char* name, int value)
{
    glad_glUniform1i(glad_glGetUniformLocation(program, name), value);
}

void SetFloat(GLuint program, const char* name, float value)
{
    glad_glUniform1f(glad_glGetUniformLocation(program, name), value);
}

void SetSize(GLuint program, int width, int height)
{
    glad_glUniform2i(glad_glGetUniformLocation(program, "uSize"), width, height);
}

struct ImageBinding {
    GLint name = 0;
    GLint level = 0;
    GLint layered = GL_FALSE;
    GLint layer = 0;
    GLint access = GL_READ_ONLY;
    GLint format = GL_R8;
};

ImageBinding CaptureImageBinding(GLuint unit)
{
    ImageBinding binding;
    glad_glGetIntegeri_v(GL_IMAGE_BINDING_NAME, unit, &binding.name);
    glad_glGetIntegeri_v(GL_IMAGE_BINDING_LEVEL, unit, &binding.level);
    glad_glGetIntegeri_v(GL_IMAGE_BINDING_LAYERED, unit, &binding.layered);
    glad_glGetIntegeri_v(GL_IMAGE_BINDING_LAYER, unit, &binding.layer);
    glad_glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, unit, &binding.access);
    glad_glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, unit, &binding.format);
    return binding;
}

void RestoreImageBinding(GLuint unit, const ImageBinding& binding)
{
    glad_glBindImageTexture(unit, static_cast<GLuint>(binding.name), binding.level,
                            binding.layered != GL_FALSE, binding.layer,
                            static_cast<GLenum>(binding.access),
                            static_cast<GLenum>(binding.format));
}

void AllocateTexture(GLuint texture, GLenum internalFormat, int width, int height)
{
    glad_glBindTexture(GL_TEXTURE_2D, texture);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const GLenum format = internalFormat == GL_R32F ? GL_RED : GL_RGBA;
    const GLenum type = internalFormat == GL_R32F ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glad_glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                      format, type, nullptr);
}

} // namespace

bool GpuNormalMapGenerator::Generate(GLuint source, GLuint& destination,
                                     int width, int height,
                                     const GpuNormalMapSettings& settings)
{
    if (!source || width <= 0 || height <= 0) return false;
    Programs& programs = GetPrograms();
    if (!programs.EnsureInitialized()) return false;

    GLint previousProgram = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    glad_glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glad_glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);

    glad_glActiveTexture(GL_TEXTURE0);
    GLint previousTexture = 0;
    glad_glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    const ImageBinding previousInputImage = CaptureImageBinding(kInputImageUnit);
    const ImageBinding previousOutputImage = CaptureImageBinding(kOutputImageUnit);

    GLuint scratch[2]{};
    glad_glGenTextures(2, scratch);
    AllocateTexture(scratch[0], GL_R32F, width, height);
    AllocateTexture(scratch[1], GL_R32F, width, height);
    if (!destination) glad_glGenTextures(1, &destination);
    AllocateTexture(destination, GL_RGBA8, width, height);

    const int radius = std::clamp(settings.blurRadius, 0, 8);
    const GLuint groupsX = (static_cast<GLuint>(width) + kLocalSize - 1) / kLocalSize;
    const GLuint groupsY = (static_cast<GLuint>(height) + kLocalSize - 1) / kLocalSize;

    glad_glUseProgram(programs.horizontal);
    glad_glBindTexture(GL_TEXTURE_2D, source);
    SetInt(programs.horizontal, "uSource", 0);
    SetSize(programs.horizontal, width, height);
    SetInt(programs.horizontal, "uRadius", radius);
    SetInt(programs.horizontal, "uUseAlpha", settings.useAlpha ? 1 : 0);
    SetInt(programs.horizontal, "uRepeat", settings.repeat ? 1 : 0);
    glad_glBindImageTexture(kOutputImageUnit, scratch[0], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, GL_R32F);
    glad_glDispatchCompute(groupsX, groupsY, 1);
    glad_glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glad_glUseProgram(programs.vertical);
    SetSize(programs.vertical, width, height);
    SetInt(programs.vertical, "uRadius", radius);
    SetInt(programs.vertical, "uRepeat", settings.repeat ? 1 : 0);
    glad_glBindImageTexture(kInputImageUnit, scratch[0], 0, GL_FALSE, 0,
                            GL_READ_ONLY, GL_R32F);
    glad_glBindImageTexture(kOutputImageUnit, scratch[1], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, GL_R32F);
    glad_glDispatchCompute(groupsX, groupsY, 1);
    glad_glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glad_glUseProgram(programs.normal);
    SetSize(programs.normal, width, height);
    SetFloat(programs.normal, "uStrength", std::max(0.0f, settings.strength));
    SetInt(programs.normal, "uUseScharr", settings.useScharr ? 1 : 0);
    SetInt(programs.normal, "uInvertX", settings.invertX ? 1 : 0);
    SetInt(programs.normal, "uInvertY", settings.invertY ? 1 : 0);
    SetInt(programs.normal, "uRepeat", settings.repeat ? 1 : 0);
    glad_glBindImageTexture(kInputImageUnit, scratch[1], 0, GL_FALSE, 0,
                            GL_READ_ONLY, GL_R32F);
    glad_glBindImageTexture(kOutputImageUnit, destination, 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, GL_RGBA8);
    glad_glDispatchCompute(groupsX, groupsY, 1);
    glad_glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                         GL_TEXTURE_FETCH_BARRIER_BIT);

    glad_glBindTexture(GL_TEXTURE_2D, destination);
    glad_glGenerateMipmap(GL_TEXTURE_2D);

    glad_glDeleteTextures(2, scratch);
    RestoreImageBinding(kInputImageUnit, previousInputImage);
    RestoreImageBinding(kOutputImageUnit, previousOutputImage);
    glad_glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glad_glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glad_glUseProgram(static_cast<GLuint>(previousProgram));
    return true;
}

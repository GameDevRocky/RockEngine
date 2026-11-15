#include <array>
#include <cstring>
#include <QImage>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "engine/core/Globals.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/utils/EngineUtils.hpp"

namespace {
// Exercise generation and draw-state binding without starting a GL viewport.
struct GlCapture {
    GLenum active = GL_TEXTURE0 + 3;
    std::array<GLuint, 32> textures{};
    GLint alignment = 4;
    GLuint nextId = 100;
    int uploads = 0;
    float hasNormal = -1.0f;
    int normalSlot = -1;
};
GlCapture* capture = nullptr;

template<typename T> struct ScopedGlFunction {
    T& target;
    T original;
    ScopedGlFunction(T& target, T replacement) : target(target), original(target) { target = replacement; }
    ~ScopedGlFunction() { target = original; }
};

void APIENTRY getInteger(GLenum name, GLint* value) {
    *value = name == GL_TEXTURE_BINDING_2D
        ? static_cast<GLint>(capture->textures[capture->active - GL_TEXTURE0]) : capture->alignment;
}
void APIENTRY genTextures(GLsizei count, GLuint* ids) {
    for (GLsizei i = 0; i < count; ++i) ids[i] = capture->nextId++;
}
void APIENTRY bindTexture(GLenum, GLuint texture) { capture->textures[capture->active - GL_TEXTURE0] = texture; }
void APIENTRY activeTexture(GLenum slot) { capture->active = slot; }
void APIENTRY deleteTextures(GLsizei, const GLuint*) {}
void APIENTRY pixelStore(GLenum, GLint alignment) { capture->alignment = alignment; }
void APIENTRY textureImage(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) { ++capture->uploads; }
void APIENTRY generateMipmap(GLenum) {}
void APIENTRY textureParameter(GLenum, GLenum, GLint) {}
GLint APIENTRY uniformLocation(GLuint, const GLchar* name) {
    if (std::strcmp(name, "uHasNormalMap") == 0) return 1;
    if (std::strcmp(name, "uNormalMap") == 0) return 2;
    return -1;
}
void APIENTRY uniformFloat(GLint location, GLfloat value) { if (location == 1) capture->hasNormal = value; }
void APIENTRY uniformInt(GLint location, GLint value) { if (location == 2) capture->normalSlot = value; }
}

TEST(TextureNormalBindingTest, GeneratesBindsAndDisablesNormalsWithoutLeakingAcrossTextures) {
    auto* globals = Globals::Get();
    const bool originalGpu = globals->Get<bool>("Rendering", "useGPUNormalMapGeneration", false);
    struct RestoreBackend {
        bool value;
        ~RestoreBackend() { Globals::Get()->SetValue("Rendering", "useGPUNormalMapGeneration", value); }
    } restore{originalGpu};
    ASSERT_TRUE(globals->SetValue("Rendering", "useGPUNormalMapGeneration", false));

    GlCapture state;
    capture = &state;
    state.textures[3] = 77; // Existing albedo binding must survive generation.
    ScopedGlFunction get(glad_glGetIntegerv, &getInteger);
    ScopedGlFunction gen(glad_glGenTextures, &genTextures);
    ScopedGlFunction bind(glad_glBindTexture, &bindTexture);
    ScopedGlFunction active(glad_glActiveTexture, &activeTexture);
    ScopedGlFunction remove(glad_glDeleteTextures, &deleteTextures);
    ScopedGlFunction store(glad_glPixelStorei, &pixelStore);
    ScopedGlFunction image(glad_glTexImage2D, &textureImage);
    ScopedGlFunction mipmap(glad_glGenerateMipmap, &generateMipmap);
    ScopedGlFunction parameter(glad_glTexParameteri, &textureParameter);
    ScopedGlFunction location(glad_glGetUniformLocation, &uniformLocation);
    ScopedGlFunction scalar(glad_glUniform1f, &uniformFloat);
    ScopedGlFunction sampler(glad_glUniform1i, &uniformInt);

    QTemporaryDir temp(QString::fromStdString(
        EngineUtils::GetAssetPath("Tests/mocks/NormalBindingXXXXXX")));
    ASSERT_TRUE(temp.isValid());
    QImage source(2, 2, QImage::Format_RGBA8888);
    source.fill(Qt::white);
    const QString path = temp.filePath("normal-source.png");
    ASSERT_TRUE(source.save(path));
    YAML::Node node;
    node["id"] = "normal-binding-test";
    node["path"] = EngineUtils::ToAssetRelative(path.toStdString());
    node["filtering"] = "nearest";
    node["wrap"] = "clamp";
    node["normal"]["apply"] = true;
    Texture2D mapped, unmapped;
    Shader shader;
    mapped.Deserialize(node);

    mapped.BindNormalMap(shader);
    ASSERT_NE(mapped.GetNormalTextureID(), 0u);
    EXPECT_EQ(state.textures[EngineUtils::TextureSlots::NormalMap], mapped.GetNormalTextureID());
    EXPECT_EQ(state.textures[3], 77u);
    EXPECT_EQ(state.alignment, 4);
    EXPECT_EQ(state.normalSlot, EngineUtils::TextureSlots::NormalMap);
    EXPECT_FLOAT_EQ(state.hasNormal, 1.0f);
    EXPECT_EQ(state.uploads, 1);
    mapped.BindNormalMap(shader);
    EXPECT_EQ(state.uploads, 1); // Cached across draws until the recipe changes.

    unmapped.BindNormalMap(shader);
    EXPECT_FLOAT_EQ(state.hasNormal, 0.0f);
    mapped.BindNormalMap(shader);
    EXPECT_FLOAT_EQ(state.hasNormal, 1.0f);
    mapped.SetApplyNormal(false);
    mapped.BindNormalMap(shader);
    EXPECT_EQ(mapped.GetNormalTextureID(), 0u);
    EXPECT_FLOAT_EQ(state.hasNormal, 0.0f);
}

#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include <tuple>
#include <vector>

#include "dock-widgets/SpriteEditor.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/utils/EngineUtils.hpp"

namespace {
// Upload only records dimensions for these tests; no viewport or GL context is needed.
template<typename T> struct ScopedGlFunction {
    T& target;
    T original;
    ScopedGlFunction(T& target, T replacement) : target(target), original(target) { target = replacement; }
    ~ScopedGlFunction() { target = original; }
};

void APIENTRY genTextures(GLsizei count, GLuint* ids) {
    for (GLsizei i = 0; i < count; ++i) ids[i] = 1;
}
void APIENTRY bindTexture(GLenum, GLuint) {}
void APIENTRY getInteger(GLenum, GLint* value) { *value = 4; }
void APIENTRY pixelStore(GLenum, GLint) {}
void APIENTRY textureImage(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {}
void APIENTRY generateMipmap(GLenum) {}
void APIENTRY textureParameter(GLenum, GLenum, GLint) {}
void APIENTRY deleteTextures(GLsizei, const GLuint*) {}

class CountingTexture : public Texture2D {
public:
    int saves = 0;
    YAML::Node Serialize() override {
        ++saves;
        return Texture2D::Serialize();
    }
};
}

class SpriteEditorTest : public ::testing::TestWithParam<std::tuple<int, bool>> {
protected:
    ScopedGlFunction<PFNGLGENTEXTURESPROC> gen{glad_glGenTextures, &genTextures};
    ScopedGlFunction<PFNGLBINDTEXTUREPROC> bind{glad_glBindTexture, &bindTexture};
    ScopedGlFunction<PFNGLGETINTEGERVPROC> get{glad_glGetIntegerv, &getInteger};
    ScopedGlFunction<PFNGLPIXELSTOREIPROC> store{glad_glPixelStorei, &pixelStore};
    ScopedGlFunction<PFNGLTEXIMAGE2DPROC> image{glad_glTexImage2D, &textureImage};
    ScopedGlFunction<PFNGLGENERATEMIPMAPPROC> mipmap{glad_glGenerateMipmap, &generateMipmap};
    ScopedGlFunction<PFNGLTEXPARAMETERIPROC> parameter{glad_glTexParameteri, &textureParameter};
    ScopedGlFunction<PFNGLDELETETEXTURESPROC> remove{glad_glDeleteTextures, &deleteTextures};
    QTemporaryDir temp{QString::fromStdString(EngineUtils::GetAssetPath("Tests/mocks/SpriteSliceXXXXXX"))};
    CountingTexture* texture = nullptr;
    std::string descriptor;
    SpriteEditor editor;

    void SetUp() override {
        ASSERT_TRUE(temp.isValid());
        const QString sourcePath = temp.filePath("sheet.png");
        QImage source(100, 200, QImage::Format_RGBA8888);
        source.fill(Qt::white);
        ASSERT_TRUE(source.save(sourcePath));
        descriptor = temp.filePath("sheet.png.texture").toStdString();

        YAML::Node node;
        node["id"] = "sprite-slice-test";
        node["path"] = EngineUtils::ToAssetRelative(sourcePath.toStdString());
        node["filtering"] = "nearest";
        node["wrap"] = "clamp";
        texture = new CountingTexture();
        texture->Deserialize(node);
        texture->SetFilePath(descriptor);
        texture->Awake();
        AssetManager::Get().AddTexture(texture);
        editor.SetTexture(texture);
    }

    void TearDown() override {
        editor.SetTexture(nullptr);
        if (texture) AssetManager::Get().RemoveAsset(texture->GetID());
    }
};

TEST_P(SpriteEditorTest, GridSliceSavesOnceAndPreservesSpriteDefinitions) {
    const auto [mode, replaceExisting] = GetParam();
    auto& assets = AssetManager::Get();
    std::vector<std::string> originalIds;
    for (int i = 0; i < 2; ++i) {
        auto* sprite = assets.CreateSprite(texture->GetID(), {0, 0}, {1, 1}, "original");
        ASSERT_NE(sprite, nullptr);
        originalIds.push_back(sprite->GetID());
    }

    auto* modePicker = editor.findChild<QComboBox*>();
    const auto dimensions = editor.findChildren<QSpinBox*>();
    auto* prefix = editor.findChild<QLineEdit*>();
    ASSERT_NE(modePicker, nullptr);
    ASSERT_EQ(dimensions.size(), 2);
    ASSERT_NE(prefix, nullptr);
    modePicker->setCurrentIndex(mode);
    dimensions[0]->setValue(10);
    dimensions[1]->setValue(mode == 0 ? 10 : 20); // 10x10 cells in either mode.
    prefix->setText("cut");
    for (auto* checkbox : editor.findChildren<QCheckBox*>())
        if (checkbox->text() == "Replace existing") checkbox->setChecked(replaceExisting);
    QPushButton* apply = nullptr;
    for (auto* button : editor.findChildren<QPushButton*>())
        if (button->text() == "Apply") apply = button;
    ASSERT_NE(apply, nullptr);

    texture->saves = 0;
    apply->click();
    EXPECT_EQ(texture->saves, 1);

    const YAML::Node saved = YAML::LoadFile(descriptor);
    ASSERT_EQ(saved["sprites"].size(), replaceExisting ? 100u : 102u);
    for (const auto& id : originalIds) {
        EXPECT_EQ(assets.GetSprite(id) == nullptr, replaceExisting);
        bool found = false;
        for (const auto& sprite : saved["sprites"])
            if (sprite["id"].as<std::string>() == id) found = true;
        EXPECT_EQ(found, !replaceExisting);
    }
    const int offset = replaceExisting ? 0 : 2;
    for (int i = 0; i < 100; ++i) {
        const auto sprite = saved["sprites"][offset + i];
        EXPECT_EQ(sprite["name"].as<std::string>(), "cut_" + std::to_string(i));
        EXPECT_NE(assets.GetSprite(sprite["id"].as<std::string>()), nullptr);
        EXPECT_NEAR(sprite["uvMin"][0].as<float>(), (i % 10) / 10.0f, 1e-6f);
        EXPECT_NEAR(sprite["uvMin"][1].as<float>(), 1.0f - (i / 10 + 1) / 10.0f, 1e-6f);
        EXPECT_NEAR(sprite["uvMax"][0].as<float>(), (i % 10 + 1) / 10.0f, 1e-6f);
        EXPECT_NEAR(sprite["uvMax"][1].as<float>(), 1.0f - (i / 10) / 10.0f, 1e-6f);
    }

    // Ordinary edits retain their immediate persistence after a grid slice.
    texture->saves = 0;
    auto* sprite = assets.CreateSprite(texture->GetID(), {0, 0}, {1, 1}, "manual");
    ASSERT_NE(sprite, nullptr);
    EXPECT_EQ(texture->saves, 1);
    assets.RemoveSprite(sprite->GetID());
    EXPECT_EQ(texture->saves, 2);
}

INSTANTIATE_TEST_SUITE_P(SliceModes, SpriteEditorTest,
                        ::testing::Combine(::testing::Values(0, 1), ::testing::Bool()));

#include <memory>
#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/TextRenderer.hpp"

using TextRendererTest = RuntimeTest;

TEST_F(TextRendererTest, LayoutSpecAndCopyReflectAuthoredText) {
    TextRenderer renderer;
    renderer.SetText("Hello");
    renderer.SetFontSize(24.0f);
    renderer.SetMaxWidth(120.0f);
    renderer.SetLetterSpacing(2.0f);

    const TextLayoutSpec layout = renderer.GetLayoutSpec();
    EXPECT_FLOAT_EQ(layout.fontSize, 24.0f);
    EXPECT_FLOAT_EQ(layout.maxWidth, 120.0f);
    EXPECT_FLOAT_EQ(layout.letterSpacing, 2.0f);

    std::unique_ptr<TextRenderer> copy(renderer.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetText(), "Hello");
    EXPECT_FLOAT_EQ(copy->GetLayoutSpec().maxWidth, 120.0f);
}

TEST_F(TextRendererTest, LayoutSettersClampUnsafeValuesAndPreserveAlignment) {
    TextRenderer renderer;
    renderer.SetFontSize(-10.0f);
    renderer.SetMaxWidth(-1.0f);
    renderer.SetWeight(1.0f);
    renderer.SetOutlineWidth(1.0f);
    renderer.SetHAlign(TextHAlign::Right);
    renderer.SetVAlign(TextVAlign::Bottom);
    const TextLayoutSpec layout = renderer.GetLayoutSpec();
    EXPECT_FLOAT_EQ(layout.fontSize, 0.0f);
    EXPECT_FLOAT_EQ(layout.maxWidth, 0.0f);
    EXPECT_EQ(layout.hAlign, TextHAlign::Right);
    EXPECT_EQ(layout.vAlign, TextVAlign::Bottom);
    EXPECT_FLOAT_EQ(renderer.GetWeight(), 0.25f);
    EXPECT_FLOAT_EQ(renderer.GetOutlineWidth(), 0.35f);
}

TEST_F(TextRendererTest, TextChangeIsIdempotentAndCopyKeepsVisibility) {
    TextRenderer renderer;
    int textChanges = 0;
    renderer.Subscribe([&textChanges]() { ++textChanges; return true; }, TextRenderer::TEXT_CHANGED_EVENT);
    renderer.SetText("Score");
    renderer.SetText("Score");
    renderer.SetVisible(false);
    renderer.SetSortingLayer("UI");
    EXPECT_EQ(textChanges, 1);
    std::unique_ptr<TextRenderer> copy(renderer.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetText(), "Score");
    EXPECT_FALSE(copy->GetVisible());
    EXPECT_EQ(copy->GetSortingLayer(), "UI");

    TextRenderer restored;
    restored.Deserialize(renderer.Serialize());
    EXPECT_EQ(restored.GetText(), "Score");
    EXPECT_FALSE(restored.GetVisible());
    EXPECT_EQ(restored.GetSortingLayer(), "UI");
}

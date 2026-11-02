#include <memory>
#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/SpriteRenderer.hpp"

using SpriteRendererTest = RuntimeTest;

TEST_F(SpriteRendererTest, CopyKeepsVisualSettings) {
    SpriteRenderer renderer;
    renderer.SetFlipX(true);
    renderer.SetFlipY(true);
    renderer.SetUVScale({0.5f, 0.25f});
    renderer.SetUVOffset({0.25f, 0.5f});
    renderer.SetSortingOrder(7);

    std::unique_ptr<SpriteRenderer> copy(renderer.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_TRUE(copy->GetFlipX());
    EXPECT_TRUE(copy->GetFlipY());
    EXPECT_EQ(copy->GetUVScale(), glm::vec2(0.5f, 0.25f));
    EXPECT_EQ(copy->GetUVOffset(), glm::vec2(0.25f, 0.5f));
    EXPECT_EQ(copy->GetSortingOrder(), 7);
}

TEST_F(SpriteRendererTest, SerializationKeepsVisibilityColorAndTextureCoordinates) {
    SpriteRenderer renderer;
    renderer.SetColor({0.2f, 0.4f, 0.6f, 0.8f});
    renderer.SetUVOffset({0.25f, 0.75f});
    renderer.SetUVScale({2.0f, 3.0f});
    renderer.SetFlipX(true);
    renderer.SetSortingLayer("Foreground");
    bool visible = false;
    renderer.SetVisible(visible);

    SpriteRenderer restored;
    restored.Deserialize(renderer.Serialize());
    EXPECT_EQ(restored.GetColor(), renderer.GetColor());
    EXPECT_EQ(restored.GetUVOffset(), renderer.GetUVOffset());
    EXPECT_EQ(restored.GetUVScale(), renderer.GetUVScale());
    EXPECT_TRUE(restored.GetFlipX());
    EXPECT_FALSE(restored.GetVisible());
    EXPECT_EQ(restored.GetSortingLayer(), "Foreground");
}

TEST_F(SpriteRendererTest, SortingEventsFireOnlyForActualChanges) {
    SpriteRenderer renderer;
    int layerChanges = 0;
    int orderChanges = 0;
    renderer.Subscribe([&layerChanges]() { ++layerChanges; return true; }, SpriteRenderer::SORTING_LAYER_CHANGED_EVENT);
    renderer.Subscribe([&orderChanges]() { ++orderChanges; return true; }, SpriteRenderer::SORTING_ORDER_CHANGED_EVENT);
    renderer.SetSortingLayer("Default");
    renderer.SetSortingLayer("Actors");
    renderer.SetSortingLayer("Actors");
    renderer.SetSortingOrder(0);
    renderer.SetSortingOrder(3);
    renderer.SetSortingOrder(3);
    EXPECT_EQ(layerChanges, 1);
    EXPECT_EQ(orderChanges, 1);
}

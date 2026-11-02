#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/core/GameObject.hpp"

using TileMapRendererTest = RuntimeTest;

TEST_F(TileMapRendererTest, AddingRendererCreatesEditorAndGrid) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* renderer = new TileMapRenderer();
    object->AddComponent(renderer);

    TileMapEditor* editor = object->GetComponent<TileMapEditor>();
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(renderer->GetTileMapEditor(), editor);
    EXPECT_EQ(editor->GetGrid(), object->GetComponent<Grid>());
}

TEST_F(TileMapRendererTest, ReusesExistingEditorAndPersistsRenderSettings) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* editor = new TileMapEditor();
    object->AddComponent(editor);
    auto* renderer = object->GetComponent<TileMapRenderer>();
    ASSERT_NE(renderer, nullptr);
    EXPECT_EQ(renderer->GetTileMapEditor(), editor);
    renderer->SetTileSizingMode(TileMapRenderer::TileSizingMode::FillCell);
    renderer->SetSortingLayer("Foreground");
    renderer->SetSortingOrder(4);

    std::unique_ptr<TileMapRenderer> copy(renderer->Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetTileSizingMode(), TileMapRenderer::TileSizingMode::FillCell);
    EXPECT_EQ(copy->GetSortingLayer(), "Foreground");
    EXPECT_EQ(copy->GetSortingOrder(), 4);

    TileMapRenderer restored;
    restored.Deserialize(renderer->Serialize());
    EXPECT_EQ(restored.GetTileSizingMode(), TileMapRenderer::TileSizingMode::FillCell);
    EXPECT_EQ(restored.GetSortingLayer(), "Foreground");
    EXPECT_EQ(restored.GetSortingOrder(), 4);
}

#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using TileMapRendererTest = RuntimeTest;

TEST_F(TileMapRendererTest, AddingRendererCreatesEditorWithoutCreatingGrid) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* renderer = new TileMapRenderer();
    object->AddComponent(renderer);

    TileMapEditor* editor = object->GetComponent<TileMapEditor>();
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(renderer->GetTileMapEditor(), editor);
    EXPECT_EQ(object->GetComponent<Grid>(), nullptr);
    EXPECT_EQ(editor->GetGrid(), nullptr);
    EXPECT_EQ(renderer->GetGrid(), nullptr);
}

TEST_F(TileMapRendererTest, GridChangesNotifyRendererAndUseSameAssociationAsEditor) {
    auto* parent = new GameObject();
    scene->AddGameObject(parent);
    auto* object = new GameObject();
    scene->AddGameObject(object);
    object->GetTransform()->SetParent(parent->GetTransform(), false);
    auto* renderer = new TileMapRenderer();
    object->AddComponent(renderer);
    auto* editor = renderer->GetTileMapEditor();
    ASSERT_NE(editor, nullptr);
    auto notifications = std::make_shared<int>(0);
    renderer->Subscribe([notifications]() {
        ++*notifications;
        return true;
    }, TileMapRenderer::RENDER_CHANGED_EVENT);

    auto* grid = new Grid();
    parent->AddComponent(grid);
    EXPECT_EQ(renderer->GetGrid(), grid);
    EXPECT_EQ(renderer->GetGrid(), editor->GetGrid());
    EXPECT_EQ(*notifications, 1);
    object->AddComponent(new Grid());
    EXPECT_EQ(renderer->GetGrid(), grid);
    EXPECT_EQ(*notifications, 1);
    parent->RemoveComponent(grid);
    EXPECT_EQ(renderer->GetGrid(), nullptr);
    EXPECT_EQ(*notifications, 2);
    auto* replacementGrid = new Grid();
    parent->AddComponent(replacementGrid);
    EXPECT_EQ(renderer->GetGrid(), replacementGrid);
    EXPECT_EQ(*notifications, 3);
}

TEST_F(TileMapRendererTest, ReplacingTileMapEditorRestoresGridAndChangeNotifications) {
    auto* parent = new GameObject();
    scene->AddGameObject(parent);
    auto* grid = new Grid();
    parent->AddComponent(grid);
    auto* object = new GameObject();
    scene->AddGameObject(object);
    object->GetTransform()->SetParent(parent->GetTransform(), false);
    auto* renderer = new TileMapRenderer();
    object->AddComponent(renderer);
    auto* editor = renderer->GetTileMapEditor();
    ASSERT_NE(editor, nullptr);
    object->RemoveComponent(editor);
    registry->FlushPendingShutdowns();
    EXPECT_EQ(renderer->GetTileMapEditor(), nullptr);
    EXPECT_EQ(renderer->GetGrid(), nullptr);

    auto notifications = std::make_shared<int>(0);
    renderer->Subscribe([notifications]() {
        ++*notifications;
        return true;
    }, TileMapRenderer::RENDER_CHANGED_EVENT);
    auto* replacementEditor = new TileMapEditor();
    object->AddComponent(replacementEditor);
    EXPECT_EQ(renderer->GetTileMapEditor(), replacementEditor);
    EXPECT_EQ(renderer->GetGrid(), grid);
    EXPECT_EQ(*notifications, 1);
    parent->RemoveComponent(grid);
    EXPECT_EQ(renderer->GetGrid(), nullptr);
    EXPECT_EQ(*notifications, 2);
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
    renderer->SetMaterial("tile-material");
    renderer->SetSortingLayer("Foreground");
    renderer->SetSortingOrder(4);

    std::unique_ptr<TileMapRenderer> copy(renderer->Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetTileSizingMode(), TileMapRenderer::TileSizingMode::FillCell);
    EXPECT_EQ(copy->GetMaterialId(), "tile-material");
    EXPECT_EQ(copy->GetSortingLayer(), "Foreground");
    EXPECT_EQ(copy->GetSortingOrder(), 4);

    TileMapRenderer restored;
    restored.Deserialize(renderer->Serialize());
    EXPECT_EQ(restored.GetTileSizingMode(), TileMapRenderer::TileSizingMode::FillCell);
    EXPECT_EQ(restored.GetMaterialId(), "tile-material");
    EXPECT_EQ(restored.GetSortingLayer(), "Foreground");
    EXPECT_EQ(restored.GetSortingOrder(), 4);
}

TEST_F(TileMapRendererTest, LegacySceneUsesDefaultMaterial) {
    TileMapRenderer renderer;
    YAML::Node sceneNode;
    sceneNode["type"] = "TileMapRenderer";
    renderer.Deserialize(sceneNode);
    EXPECT_EQ(renderer.GetMaterialId(), "m1");
}

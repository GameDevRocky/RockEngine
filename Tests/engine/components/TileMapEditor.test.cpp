#include <gtest/gtest.h>
#include <filesystem>
#include "RuntimeFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/rendering/core/AssetManager.hpp"

class TileMapEditorTest : public RuntimeTest {
protected:
    void TearDown() override {
        RuntimeTest::TearDown();
        AssetManager::Get().RemoveAsset("test-cells-tilemap");
    }
};

TEST_F(TileMapEditorTest, AddingTileMapEditorCreatesGridAndRenderer) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* editor = new TileMapEditor();
    object->AddComponent(editor);

    Grid* grid = object->GetComponent<Grid>();
    ASSERT_NE(grid, nullptr);
    EXPECT_EQ(editor->GetGrid(), grid);
    EXPECT_EQ(grid->GetSize(), glm::vec2(32.0f, 32.0f));
    EXPECT_NE(object->GetComponent<TileMapRenderer>(), nullptr);
}

TEST_F(TileMapEditorTest, ExistingGridIsReusedWithoutChangingItsSize) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* grid = new Grid();
    object->AddComponent(grid);
    grid->SetSize({8.0f, 12.0f});
    auto* editor = new TileMapEditor();
    object->AddComponent(editor);
    EXPECT_EQ(editor->GetGrid(), grid);
    EXPECT_EQ(grid->GetSize(), glm::vec2(8.0f, 12.0f));
    EXPECT_EQ(editor->WorldToCell(editor->CellToWorld({2, -3})), glm::ivec2(2, -3));
}

TEST_F(TileMapEditorTest, EditingWithoutTileMapFailsWithoutChangingEmptyCells) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* editor = new TileMapEditor();
    object->AddComponent(editor);
    EXPECT_EQ(editor->GetTileMap(), nullptr);
    EXPECT_EQ(editor->GetTileId({1, 2}), EMPTY_TILE_ID);
    EXPECT_FALSE(editor->SetTileId({1, 2}, 7));
    EXPECT_FALSE(editor->ClearTile({1, 2}));
    for (TileId neighbor : editor->GetNeighbors({1, 2})) EXPECT_EQ(neighbor, EMPTY_TILE_ID);
}

TEST_F(TileMapEditorTest, LoadedTileMapEditsCellsAndReadsNeighbors) {
    auto* map = new TileMap();
    map->SetID("test-cells-tilemap");
    map->SetFilePath((std::filesystem::path(PROJECT_ROOT) / "Tests" / "mocks" / "cells.tilemap").string());
    AssetManager::Get().AddTileMap(map);
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* editor = new TileMapEditor();
    object->AddComponent(editor);
    editor->SetTileMap(map);
    ASSERT_NE(map->GetCells(), nullptr);

    EXPECT_EQ(editor->GetTileId({1, 0}), 4);
    EXPECT_EQ(editor->GetNeighbors({0, 0})[0], 8);
    EXPECT_EQ(editor->GetNeighbors({0, 0})[2], 4);
    EXPECT_TRUE(editor->SetTileId({2, 0}, 9));
    EXPECT_FALSE(editor->SetTileId({2, 0}, 9));
    EXPECT_EQ(editor->GetTileId({2, 0}), 9);
    EXPECT_TRUE(editor->ClearTile({2, 0}));
    EXPECT_FALSE(editor->ClearTile({2, 0}));
    EXPECT_EQ(editor->GetTileId({2, 0}), EMPTY_TILE_ID);
}

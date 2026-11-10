#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "EditorSceneFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/GizmosManager.hpp"
#include "engine/tilemap/TileMap.hpp"
#include "utils/SceneEditController.hpp"
#include "utils/SceneEditTools.hpp"
#include "utils/TilePaintState.hpp"

class TilemapPaintToolTest : public EditorSceneTest {};

TEST_F(TilemapPaintToolTest, RightButtonTemporarilyErasesWithoutChangingSelectedTile)
{
    const auto sourcePath = std::filesystem::path(PROJECT_ROOT) / "Tests" / "mocks" / "cells.tilemap";
    const auto tempPath = std::filesystem::temp_directory_path() / "rockengine-right-erase.tilemap";
    TilePaintState& paintState = TilePaintState::Get();
    GizmosManager* gizmos = GizmosManager::Get();
    struct Cleanup {
        std::filesystem::path tempPath;
        std::string mapId;
        TileMapEditor* editor = nullptr;
        std::string componentId;
        TileId tileId;
        int brushSize;
        GizmosManager* gizmos;
        GizmosManager::EditMode editMode;
        ~Cleanup()
        {
            if (editor) editor->SetTileMap(nullptr);
            if (!mapId.empty() && AssetManager::Get().GetTileMap(mapId))
                AssetManager::Get().RemoveAsset(mapId);
            std::error_code error;
            std::filesystem::remove(tempPath, error);
            TilePaintState::Get().componentId = componentId;
            TilePaintState::Get().selectedTileId = tileId;
            TilePaintState::Get().brushSize = brushSize;
            gizmos->SetEditMode(editMode);
        }
    } cleanup{tempPath, {}, nullptr, paintState.componentId, paintState.selectedTileId,
              paintState.brushSize, gizmos, gizmos->GetEditMode()};

    std::filesystem::copy_file(sourcePath, tempPath, std::filesystem::copy_options::overwrite_existing);

    auto* map = new TileMap();
    map->SetID("right-erase-test-map");
    map->SetFilePath(tempPath.string());
    AssetManager::Get().AddTileMap(map);
    cleanup.mapId = map->GetID();

    auto* gridObject = new GameObject();
    scene->AddGameObject(gridObject);
    gridObject->AddComponent(new Grid());
    auto* tileObject = new GameObject();
    scene->AddGameObject(tileObject);
    tileObject->GetTransform()->SetParent(gridObject->GetTransform(), false);
    auto* editor = new TileMapEditor();
    tileObject->AddComponent(editor);
    cleanup.editor = editor;
    editor->SetTileMap(map);
    ASSERT_NE(map->GetCells(), nullptr);
    ASSERT_TRUE(editor->SetTileId({0, 0}, 7));

    auto* selection = editorContainer->FindSystem<SelectionManager>();
    ASSERT_NE(selection, nullptr);
    selection->Select(tileObject->GetID());

    paintState.componentId = editor->GetID();
    paintState.selectedTileId = 99;
    paintState.brushSize = 1;

    gizmos->SetEditMode(GizmosManager::EditMode::Tilemap);

    auto tool = CreateTilemapPaintTool();
    SceneEditContext context;
    context.container = editorContainer;
    SceneEditPointerEvent event;
    event.button = Qt::RightButton;
    event.buttons = Qt::RightButton;
    event.inside = true;

    EXPECT_TRUE(tool->PointerPress(context, event));
    EXPECT_EQ(editor->GetTileId({0, 0}), EMPTY_TILE_ID);
    EXPECT_EQ(paintState.selectedTileId, 99);

    event.button = Qt::NoButton;
    EXPECT_TRUE(tool->PointerMove(context, event));
    event.button = Qt::RightButton;
    event.buttons = Qt::NoButton;
    EXPECT_TRUE(tool->PointerRelease(context, event));
}

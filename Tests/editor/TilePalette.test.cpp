#include <QListWidget>
#include <QDoubleSpinBox>
#include <gtest/gtest.h>

#include "EditorSceneFixture.hpp"
#include "dock-widgets/SceneToolbars.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/tilemap/TileSet.hpp"
#include "utils/TilePaintState.hpp"

class TilePaletteTest : public EditorSceneTest {};

TEST_F(TilePaletteTest, ShowsEraseTileAndControlsBrushSize) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* editor = new TileMapEditor();
    object->AddComponent(editor);

    SceneTilePalette palette(nullptr);
    palette.SetEditor(editor->GetID());
    auto* tiles = palette.findChild<QListWidget*>("SceneTilePaletteList");
    auto* brush = palette.findChild<QDoubleSpinBox*>("SceneTileBrushSize");
    ASSERT_NE(tiles, nullptr);
    ASSERT_NE(brush, nullptr);
    ASSERT_EQ(tiles->count(), 1);
    EXPECT_EQ(tiles->item(0)->text(), "None");
    EXPECT_EQ(TilePaintState::Get().componentId, editor->GetID());

    brush->setValue(3);
    EXPECT_EQ(TilePaintState::Get().brushSize, 3);
    tiles->setCurrentRow(0);
    EXPECT_EQ(TilePaintState::Get().selectedTileId, EMPTY_TILE_ID);

    palette.SetEditor({});
    EXPECT_TRUE(TilePaintState::Get().componentId.empty());
    TilePaintState::Get().brushSize = 1;
}

TEST_F(TilePaletteTest, SelectingTileChangesPaintAndNoneErases) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* editor = new TileMapEditor();
    object->AddComponent(editor);

    auto* tileSet = new TileSet();
    Tile tile;
    tile.name = "Grass";
    tileSet->SetTile(7, tile);
    const std::string tileSetId = tileSet->GetID();
    AssetManager::Get().AddTileSet(tileSet);
    struct RemoveTileSet {
        std::string id;
        ~RemoveTileSet() { AssetManager::Get().RemoveAsset(id); }
    } cleanup{tileSetId};
    editor->SetTileSet(tileSet);

    SceneTilePalette palette(nullptr);
    palette.SetEditor(editor->GetID());
    auto* tiles = palette.findChild<QListWidget*>("SceneTilePaletteList");
    ASSERT_NE(tiles, nullptr);
    ASSERT_EQ(tiles->count(), 2);
    EXPECT_EQ(tiles->currentItem()->data(Qt::UserRole).toUInt(), 7u);

    tiles->setCurrentRow(0);
    EXPECT_EQ(TilePaintState::Get().selectedTileId, EMPTY_TILE_ID);
    tiles->setCurrentRow(1);
    EXPECT_EQ(TilePaintState::Get().selectedTileId, 7);

    palette.SetEditor({});
    editor->SetTileSet(nullptr);
}

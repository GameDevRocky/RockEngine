#include <algorithm>
#include <filesystem>
#include <memory>

#include <gtest/gtest.h>
#include <QJsonArray>

#include "EditorSceneFixture.hpp"
#include "mcp/McpDispatcher.hpp"
#include "mcp/Tools.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapCollider.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/tilemap/TileMap.hpp"
#include "engine/tilemap/TileSet.hpp"

namespace {
QJsonObject Call(mcp::McpDispatcher& dispatcher, const char* method, const QJsonObject& params = {}) {
    return dispatcher.Dispatch(QJsonObject{{"jsonrpc", "2.0"}, {"id", 1},
                                           {"method", method}, {"params", params}});
}
QJsonObject Data(const QJsonObject& response) {
    return response.value("result").toObject().value("data").toObject();
}
}

class TileMcpTest : public EditorSceneTest {};

TEST_F(TileMcpTest, EditsTileDefinitionsAndMapCellsThroughDiscoverableComponents) {
    namespace fs = std::filesystem;
    const fs::path setPath = fs::temp_directory_path() / "rockengine-mcp-test.tileset";
    const fs::path mapPath = fs::temp_directory_path() / "rockengine-mcp-test.tilemap";
    fs::copy_file(fs::path(PROJECT_ROOT) / "Tests/mocks/cells.tilemap", mapPath,
                  fs::copy_options::overwrite_existing);
    auto* set = new TileSet();
    set->SetID("mcp-test-tileset");
    set->SetFilePath(setPath.string());
    AssetManager::Get().AddTileSet(set);
    auto* map = new TileMap();
    map->SetID("mcp-test-tilemap");
    map->SetFilePath(mapPath.string());
    AssetManager::Get().AddTileMap(map);

    struct Cleanup {
        TileMapEditor* editor = nullptr;
        fs::path setPath, mapPath;
        Container* container;
        ~Cleanup() {
            if (auto* undo = container->FindSystem<UndoSystem>()) undo->Clear();
            if (editor) { editor->SetTileMap(nullptr); editor->SetTileSet(nullptr); }
            AssetManager::Get().RemoveAsset("mcp-test-tilemap");
            AssetManager::Get().RemoveAsset("mcp-test-tileset");
            std::error_code error;
            fs::remove(setPath, error);
            fs::remove(mapPath, error);
        }
    } cleanup{nullptr, setPath, mapPath, editorContainer};

    mcp::McpDispatcher dispatcher;
    mcp::RegisterAssetTools(dispatcher);
    mcp::RegisterPropertyTools(dispatcher);
    mcp::RegisterTileTools(dispatcher);

    auto listed = Data(Call(dispatcher, "assets.list", {{"type", "tileset"}}));
    EXPECT_EQ(listed.value("tileSets").toObject().value("total").toInt(), 1);
    EXPECT_EQ(Data(Call(dispatcher, "asset.describe", {{"assetId", "mcp-test-tilemap"}})).value("type"), "TileMap");

    const QJsonObject tile{{"name", "Ground"}, {"defaultSpriteId", ""},
                           {"collisionType", "Grid"}, {"rules", QJsonArray{}}};
    EXPECT_FALSE(Call(dispatcher, "tileset.set_tile", {{"assetId", "mcp-test-tileset"},
        {"tileId", 7}, {"tile", tile}}).contains("error"));
    EXPECT_TRUE(fs::exists(setPath));
    EXPECT_EQ(Data(Call(dispatcher, "tileset.get_tile", {{"assetId", "mcp-test-tileset"},
        {"tileId", 7}})).value("name"), "Ground");
    EXPECT_EQ(Data(Call(dispatcher, "asset.describe", {{"assetId", "mcp-test-tileset"}})).value("type"), "TileSet");
    QJsonObject invalidTile = tile;
    invalidTile["customPolygon"] = QJsonArray{QJsonArray{0.0, 0.0}, QJsonArray{1.0, 0.0}, QJsonArray{0.0, 1.0}};
    EXPECT_TRUE(Call(dispatcher, "tileset.set_tile", {{"assetId", "mcp-test-tileset"},
        {"tileId", 7}, {"tile", invalidTile}}).contains("error"));
    EXPECT_EQ(set->GetTile(7)->name, "Ground");

    auto* gridObject = new GameObject();
    scene->AddGameObject(gridObject);
    gridObject->AddComponent(new Grid());
    auto* tileObject = new GameObject();
    scene->AddGameObject(tileObject);
    tileObject->GetTransform()->SetParent(gridObject->GetTransform(), false);
    auto* editor = new TileMapEditor();
    tileObject->AddComponent(editor);
    cleanup.editor = editor;
    auto* renderer = tileObject->GetComponent<TileMapRenderer>();
    ASSERT_NE(renderer, nullptr);

    EXPECT_FALSE(Call(dispatcher, "component.set_properties", {{"componentId", QString::fromStdString(editor->GetID())},
        {"values", QJsonObject{{"tilemap", "mcp-test-tilemap"}, {"tileset", "mcp-test-tileset"}}}}).contains("error"));
    EXPECT_EQ(editor->GetTileMap(), map);
    EXPECT_EQ(editor->GetTileSet(), set);
    EXPECT_FALSE(Call(dispatcher, "component.set_property", {{"componentId", QString::fromStdString(renderer->GetID())},
        {"property", "sizing"}, {"value", "FillCell"}}).contains("error"));
    EXPECT_EQ(renderer->GetTileSizingMode(), TileMapRenderer::TileSizingMode::FillCell);
    const auto rendererProperties = Data(Call(dispatcher, "component.describe",
        {{"componentId", QString::fromStdString(renderer->GetID())}})).value("properties").toArray();
    EXPECT_TRUE(std::any_of(rendererProperties.begin(), rendererProperties.end(), [](const QJsonValue& value) {
        return value.toObject().value("name") == "color";
    }));
    EXPECT_FALSE(Call(dispatcher, "component.set_property", {{"componentId", QString::fromStdString(renderer->GetID())},
        {"property", "color"}, {"value", QJsonArray{0.2, 0.4, 0.6, 0.5}}}).contains("error"));
    EXPECT_EQ(renderer->GetColor(), glm::vec4(0.2f, 0.4f, 0.6f, 0.5f));
    auto* collider = new TileMapCollider();
    tileObject->AddComponent(collider);
    const QJsonArray colliderProps = Data(Call(dispatcher, "component.describe", {{"componentId", QString::fromStdString(collider->GetID())}}))
        .value("properties").toArray();
    EXPECT_TRUE(std::any_of(colliderProps.begin(), colliderProps.end(), [](const QJsonValue& value) {
        return value.toObject().value("name") == "friction";
    }));
    EXPECT_FALSE(Call(dispatcher, "component.set_property", {{"componentId", QString::fromStdString(collider->GetID())},
        {"property", "friction"}, {"value", 0.75}}).contains("error"));
    EXPECT_FLOAT_EQ(collider->GetFriction(), 0.75f);

    const QJsonObject paint{{"componentId", QString::fromStdString(editor->GetID())},
                            {"cells", QJsonArray{QJsonObject{{"x", 4}, {"y", 5}, {"tileId", 7}}}}};
    const QJsonObject invalidPaint{{"componentId", QString::fromStdString(editor->GetID())},
        {"cells", QJsonArray{QJsonObject{{"x", 4}, {"y", 5}, {"tileId", 7}},
                             QJsonObject{{"x", 8}, {"y", 9}, {"tileId", 999}}}}};
    EXPECT_TRUE(Call(dispatcher, "tilemap.set_cells", invalidPaint).contains("error"));
    EXPECT_EQ(map->GetTileId({4, 5}), 0);
    EXPECT_FALSE(Call(dispatcher, "tilemap.set_cells", paint).contains("error"));
    EXPECT_EQ(map->GetTileId({4, 5}), 7);
    auto result = Data(Call(dispatcher, "tilemap.get_cells", {{"assetId", "mcp-test-tilemap"},
        {"minX", 4}, {"maxX", 4}, {"minY", 5}, {"maxY", 5}}));
    EXPECT_EQ(result.value("total").toInt(), 1);
    editorContainer->FindSystem<UndoSystem>()->Undo();
    EXPECT_EQ(map->GetTileId({4, 5}), 0);
}

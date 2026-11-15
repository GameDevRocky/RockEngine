#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "RuntimeFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/TileMapCollider.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/tilemap/TileMap.hpp"
#include "engine/tilemap/TileSet.hpp"

class TileMapColliderTest : public RuntimeTest {
protected:
    std::filesystem::path mapPath;
    std::string mapId = "tilemap-collider-test-map";
    std::string tileSetId = "tilemap-collider-test-set";
    std::vector<std::string> extraAssetIds;

    void SetUp() override {
        RuntimeTest::SetUp();
        mapPath = std::filesystem::temp_directory_path() / "rockengine-tilemap-collider-test.tilemap";
        std::ofstream file(mapPath);
        file << "type: TileMap\n"
                "id: tilemap-collider-test-map\n"
                "cells:\n"
                "  - { x: 0, y: 0, tileId: 1 }\n"
                "  - { x: 1, y: 0, tileId: 1 }\n"
                "  - { x: 0, y: 1, tileId: 2 }\n";
        file.close();

        auto* map = new TileMap();
        map->SetID(mapId);
        map->SetFilePath(mapPath.string());
        AssetManager::Get().AddTileMap(map);

        auto* set = new TileSet();
        set->SetID(tileSetId);
        Tile solid;
        solid.name = "Solid";
        solid.collisionType = TileCollisionType::Grid;
        set->SetTile(1, solid);
        Tile decorative;
        decorative.name = "Decorative";
        set->SetTile(2, decorative);
        AssetManager::Get().AddTileSet(set);
    }

    void TearDown() override {
        RuntimeTest::TearDown();
        AssetManager::Get().RemoveAsset(mapId);
        AssetManager::Get().RemoveAsset(tileSetId);
        for (const std::string& id : extraAssetIds) AssetManager::Get().RemoveAsset(id);
        std::error_code error;
        std::filesystem::remove(mapPath, error);
    }

    struct TileMapObject {
        GameObject* root;
        GameObject* object;
        Grid* grid;
        TileMapEditor* editor;
        TileMapCollider* collider;
    };

    TileMapObject AddTileMapObject(RigidBody* existingBody = nullptr) {
        auto* root = new GameObject();
        scene->AddGameObject(root);
        auto* grid = new Grid();
        root->AddComponent(grid);
        grid->SetSize({32.0f, 32.0f});

        auto* object = new GameObject();
        scene->AddGameObject(object);
        object->GetTransform()->SetParent(root->GetTransform(), false);
        if (existingBody) object->AddComponent(existingBody);
        auto* editor = new TileMapEditor();
        object->AddComponent(editor);
        editor->SetTileMap(AssetManager::Get().GetTileMap(mapId));
        editor->SetTileSet(AssetManager::Get().GetTileSet(tileSetId));
        auto* collider = new TileMapCollider();
        object->AddComponent(collider);
        return {root, object, grid, editor, collider};
    }
};

TEST_F(TileMapColliderTest, GeneratesOnlySolidTilesAndMergesAcrossZeroGap) {
    const auto result = AddTileMapObject();
    ASSERT_EQ(result.collider->GetGeneratedShapeCount(), 1u);
    ASSERT_EQ(result.collider->GetGeneratedRectangles().size(), 1u);
    EXPECT_EQ(result.collider->GetGeneratedRectangles()[0].center, glm::vec2(32.0f, 16.0f));
    EXPECT_EQ(result.collider->GetGeneratedRectangles()[0].size, glm::vec2(64.0f, 32.0f));
    ASSERT_NE(result.object->GetComponent<RigidBody>(), nullptr);
    EXPECT_EQ(result.object->GetComponent<RigidBody>()->GetBodyType(), b2_staticBody);

    result.object->GetTransform()->SetScale({2.0f, 1.0f});
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    result.collider->FixedUpdate();
    b2ShapeId shape{};
    RigidBody* body = result.object->GetComponent<RigidBody>();
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), &shape, 1), 1);
    const b2Polygon polygon = b2Shape_GetPolygon(shape);
    float minimumX = polygon.vertices[0].x;
    float maximumX = polygon.vertices[0].x;
    for (int index = 1; index < polygon.count; ++index) {
        minimumX = std::min(minimumX, polygon.vertices[index].x);
        maximumX = std::max(maximumX, polygon.vertices[index].x);
    }
    EXPECT_NEAR(maximumX - minimumX, 4.0f, 0.001f);
}

TEST_F(TileMapColliderTest, ObserverEventsBatchUntilFixedUpdateAndPreserveGridGaps) {
    const auto result = AddTileMapObject();
    TileMap* map = result.editor->GetTileMap();
    ASSERT_NE(map, nullptr);
    ASSERT_TRUE(map->SetTileId({2, 0}, 1));
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    EXPECT_EQ(result.collider->GetGeneratedRectangles()[0].size.x, 64.0f);

    result.collider->FixedUpdate();
    EXPECT_FALSE(result.collider->IsGeometryDirty());
    ASSERT_EQ(result.collider->GetGeneratedRectangles().size(), 1u);
    EXPECT_EQ(result.collider->GetGeneratedRectangles()[0].size.x, 96.0f);

    result.grid->SetGap({4.0f, 0.0f});
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    result.collider->FixedUpdate();
    ASSERT_EQ(result.collider->GetGeneratedRectangles().size(), 3u);
    EXPECT_EQ(result.collider->GetGeneratedShapeCount(), 3u);
    for (const auto& rectangle : result.collider->GetGeneratedRectangles())
        EXPECT_EQ(rectangle.size, glm::vec2(32.0f, 32.0f));
}

TEST_F(TileMapColliderTest, TileSetChangesAndExistingBodyTypeRefreshSafely) {
    auto* body = new RigidBody();
    const auto result = AddTileMapObject(body);
    body->SetBodyType(b2_dynamicBody);
    EXPECT_EQ(body->GetBodyType(), b2_dynamicBody);

    TileSet* set = result.editor->GetTileSet();
    ASSERT_NE(set, nullptr);
    Tile solid = *set->GetTile(1);
    solid.collisionType = TileCollisionType::None;
    ASSERT_TRUE(set->SetTile(1, solid));
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    EXPECT_EQ(result.collider->GetGeneratedShapeCount(), 1u);
    result.collider->FixedUpdate();
    EXPECT_EQ(result.collider->GetGeneratedShapeCount(), 0u);
    EXPECT_TRUE(result.collider->GetGeneratedRectangles().empty());
}

TEST_F(TileMapColliderTest, ReplacingTileSetDetachesTheOldObserver) {
    const auto result = AddTileMapObject();
    TileSet* original = result.editor->GetTileSet();
    ASSERT_NE(original, nullptr);

    auto* replacement = new TileSet();
    replacement->SetID("tilemap-collider-replacement-set");
    Tile replacementTile;
    replacement->SetTile(1, replacementTile);
    AssetManager::Get().AddTileSet(replacement);
    extraAssetIds.push_back(replacement->GetID());

    result.editor->SetTileSet(replacement);
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    result.collider->FixedUpdate();
    EXPECT_EQ(result.collider->GetGeneratedShapeCount(), 0u);
    EXPECT_FALSE(result.collider->IsGeometryDirty());

    Tile oldTile = *original->GetTile(1);
    oldTile.collisionType = TileCollisionType::None;
    original->SetTile(1, oldTile);
    EXPECT_FALSE(result.collider->IsGeometryDirty());

    replacementTile.collisionType = TileCollisionType::Grid;
    replacement->SetTile(1, replacementTile);
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    result.collider->FixedUpdate();
    EXPECT_EQ(result.collider->GetGeneratedShapeCount(), 1u);
}

TEST_F(TileMapColliderTest, MaterialAndSensorSettingsApplyToEveryGeneratedShape) {
    const auto result = AddTileMapObject();
    result.grid->SetGap({4.0f, 0.0f});
    result.collider->FixedUpdate();
    ASSERT_EQ(result.collider->GetGeneratedShapeCount(), 2u);

    result.collider->SetDensity(2.5f);
    result.collider->SetFriction(0.4f);
    result.collider->SetBounciness(0.3f);
    result.collider->SetIsSensor(true);
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    result.collider->FixedUpdate();

    RigidBody* body = result.object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    std::vector<b2ShapeId> shapes(static_cast<std::size_t>(b2Body_GetShapeCount(body->GetBodyId())));
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), shapes.data(), static_cast<int>(shapes.size())), 2);
    for (b2ShapeId shape : shapes) {
        EXPECT_FLOAT_EQ(b2Shape_GetDensity(shape), 2.5f);
        EXPECT_FLOAT_EQ(b2Shape_GetFriction(shape), 0.4f);
        EXPECT_FLOAT_EQ(b2Shape_GetRestitution(shape), 0.3f);
        EXPECT_TRUE(b2Shape_IsSensor(shape));
    }
}

TEST_F(TileMapColliderTest, RuleSpriteCollisionUsesRenderedBoundsWithoutMergingCells) {
    const auto result = AddTileMapObject();
    auto* sprite = new Sprite();
    sprite->SetID("tilemap-collider-sprite");
    sprite->SetPivot({0.0f, 0.0f});
    AssetManager::Get().AddSprite(sprite);
    extraAssetIds.push_back(sprite->GetID());

    TileSet* set = result.editor->GetTileSet();
    ASSERT_NE(set, nullptr);
    Tile tile = *set->GetTile(1);
    AutoTileRule rule;
    rule.spriteId = sprite->GetID();
    rule.collisionType = TileCollisionType::Sprite;
    tile.autoTileRules = {rule};
    set->SetTile(1, tile);

    TileMapRenderer* renderer = result.object->GetComponent<TileMapRenderer>();
    ASSERT_NE(renderer, nullptr);
    renderer->SetTileSizingMode(TileMapRenderer::TileSizingMode::FillCell);
    EXPECT_TRUE(result.collider->IsGeometryDirty());
    result.collider->FixedUpdate();
    ASSERT_EQ(result.collider->GetGeneratedShapeCount(), 2u);
    ASSERT_EQ(result.collider->GetGeneratedRectangles().size(), 2u);
    for (const auto& rectangle : result.collider->GetGeneratedRectangles())
        EXPECT_EQ(rectangle.size, glm::vec2(32.0f, 32.0f));
}

TEST_F(TileMapColliderTest, CopyAndSerializationPreserveInheritedColliderSettings) {
    const auto result = AddTileMapObject();
    result.collider->SetCenter({3.0f, 5.0f});
    result.collider->SetDensity(2.0f);
    result.collider->SetFriction(0.4f);
    result.collider->SetBounciness(0.25f);
    result.collider->SetRollingResistance(0.1f);
    result.collider->SetIsSensor(true);

    const YAML::Node saved = result.collider->Serialize();
    EXPECT_EQ(saved["type"].as<std::string>(), "TileMapCollider");
    std::unique_ptr<TileMapCollider> copy(result.collider->Copy());
    EXPECT_EQ(copy->GetCenter(), glm::vec2(3.0f, 5.0f));
    EXPECT_FLOAT_EQ(copy->GetDensity(), 2.0f);
    EXPECT_FLOAT_EQ(copy->GetFriction(), 0.4f);
    EXPECT_FLOAT_EQ(copy->GetBounciness(), 0.25f);
    EXPECT_FLOAT_EQ(copy->GetRollingResistance(), 0.1f);
    EXPECT_TRUE(copy->GetIsSensor());
    EXPECT_TRUE(copy->IsGeometryDirty());
}

TEST_F(TileMapColliderTest, CustomPolygonStaysWithinCellAndCreatesBox2DShapes) {
    const auto result = AddTileMapObject();
    TileSet* set = result.editor->GetTileSet();
    ASSERT_NE(set, nullptr);
    Tile tile = *set->GetTile(1);
    tile.collisionType = TileCollisionType::Custom;
    tile.customPolygon = {{0.0f, -0.5f}, {0.5f, 0.0f},
                          {0.0f, 0.5f}, {-0.5f, 0.0f}};
    ASSERT_TRUE(set->SetTile(1, tile));
    result.collider->FixedUpdate();

    EXPECT_TRUE(result.collider->GetGeneratedRectangles().empty());
    ASSERT_EQ(result.collider->GetGeneratedPolygons().size(), 2u);
    EXPECT_EQ(result.collider->GetGeneratedShapeCount(), 2u);
    bool foundDiamond = false;
    for (const auto& generated : result.collider->GetGeneratedPolygons()) {
        ASSERT_EQ(generated.points.size(), 4u);
        if (generated.points[0] == glm::vec2(16.0f, 0.0f) &&
            generated.points[1] == glm::vec2(32.0f, 16.0f)) foundDiamond = true;
    }
    EXPECT_TRUE(foundDiamond);

    RigidBody* body = result.object->GetComponent<RigidBody>();
    ASSERT_NE(body, nullptr);
    std::vector<b2ShapeId> shapes(2);
    ASSERT_EQ(b2Body_GetShapes(body->GetBodyId(), shapes.data(), 2), 2);
    for (b2ShapeId shape : shapes) EXPECT_EQ(b2Shape_GetPolygon(shape).count, 4);
}

TEST_F(TileMapColliderTest, RuleCustomPolygonUsesRenderedSpritePivot) {
    const auto result = AddTileMapObject();
    auto* sprite = new Sprite();
    sprite->SetID("tilemap-custom-collider-sprite");
    sprite->SetPivot({0.25f, -0.25f});
    AssetManager::Get().AddSprite(sprite);
    extraAssetIds.push_back(sprite->GetID());

    TileSet* set = result.editor->GetTileSet();
    ASSERT_NE(set, nullptr);
    Tile tile = *set->GetTile(1);
    AutoTileRule rule;
    rule.spriteId = sprite->GetID();
    rule.collisionType = TileCollisionType::Custom;
    rule.customPolygon = DefaultTileCollisionPolygon();
    tile.autoTileRules = {rule};
    ASSERT_TRUE(set->SetTile(1, tile));
    TileMapRenderer* renderer = result.object->GetComponent<TileMapRenderer>();
    ASSERT_NE(renderer, nullptr);
    renderer->SetTileSizingMode(TileMapRenderer::TileSizingMode::FillCell);
    result.collider->FixedUpdate();

    ASSERT_EQ(result.collider->GetGeneratedPolygons().size(), 2u);
    bool foundPivotOffset = false;
    for (const auto& polygon : result.collider->GetGeneratedPolygons()) {
        ASSERT_EQ(polygon.points.size(), 4u);
        if (polygon.points[0] == glm::vec2(-8.0f, 8.0f) &&
            polygon.points[1] == glm::vec2(24.0f, 8.0f)) foundPivotOffset = true;
    }
    EXPECT_TRUE(foundPivotOffset);
}

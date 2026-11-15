#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/tilemap/TileRender.hpp"

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
    const glm::vec4 color(0.2f, 0.4f, 0.6f, 0.8f);
    renderer->SetColor(color);
    renderer->SetSortingLayer("Foreground");
    renderer->SetSortingOrder(4);

    std::unique_ptr<TileMapRenderer> copy(renderer->Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetTileSizingMode(), TileMapRenderer::TileSizingMode::FillCell);
    EXPECT_EQ(copy->GetMaterialId(), "tile-material");
    EXPECT_EQ(copy->GetColor(), color);
    EXPECT_EQ(copy->GetSortingLayer(), "Foreground");
    EXPECT_EQ(copy->GetSortingOrder(), 4);

    TileMapRenderer restored;
    restored.Deserialize(renderer->Serialize());
    EXPECT_EQ(restored.GetTileSizingMode(), TileMapRenderer::TileSizingMode::FillCell);
    EXPECT_EQ(restored.GetMaterialId(), "tile-material");
    EXPECT_EQ(restored.GetColor(), color);
    EXPECT_EQ(restored.GetSortingLayer(), "Foreground");
    EXPECT_EQ(restored.GetSortingOrder(), 4);
}

TEST_F(TileMapRendererTest, LegacySceneUsesDefaultMaterial) {
    TileMapRenderer renderer;
    YAML::Node sceneNode;
    sceneNode["type"] = "TileMapRenderer";
    renderer.Deserialize(sceneNode);
    EXPECT_EQ(renderer.GetMaterialId(), "m1");
    EXPECT_EQ(renderer.GetColor(), glm::vec4(1.0f));
}

TEST_F(TileMapRendererTest, ColorChangesNotifyAndIgnoreSameValueAssignments) {
    TileMapRenderer renderer;
    int notifications = 0;
    const int subscription = renderer.Subscribe([&notifications]() {
        ++notifications;
        return true;
    }, TileMapRenderer::RENDER_CHANGED_EVENT);
    const glm::vec4 color(0.2f, 0.4f, 0.6f, 0.5f);
    renderer.SetColor(color);
    EXPECT_EQ(renderer.GetColor(), color);
    EXPECT_EQ(notifications, 1);
    renderer.SetColor(color);
    EXPECT_EQ(notifications, 1);
    renderer.Unsubscribe(subscription);
}

TEST_F(TileMapRendererTest, BatchedLocalVerticesPreserveTileGeometryAndNormalBasis) {
    auto* parent = new GameObject();
    scene->AddGameObject(parent);
    parent->GetTransform()->SetPosition({40.0f, 20.0f});
    parent->GetTransform()->SetRotation(30.0f);
    parent->GetTransform()->SetScale({2.0f, 3.0f});
    auto* grid = new Grid();
    parent->AddComponent(grid);
    grid->SetSize({16.0f, 24.0f});
    grid->SetGap({3.0f, 5.0f});

    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* transform = object->GetTransform();
    transform->SetParent(parent->GetTransform(), false);
    transform->SetPosition({5.0f, -8.0f});
    transform->SetRotation(45.0f);
    transform->SetScale({-1.0f, 0.5f});
    auto* renderer = new TileMapRenderer();
    object->AddComponent(renderer);
    const glm::ivec2 cell(-2, 3);
    const glm::vec2 center = TileRender::CellCenter(grid, cell);
    EXPECT_EQ(center, glm::vec2(-30.0f, 99.0f));
    const glm::mat4 mapModel = transform->GetWorldMatrix();
    const glm::mat4 tileModel = TileRender::Model(grid, renderer, cell);

    const glm::vec2 size(40.0f, 12.0f), pivot(0.2f, 0.3f);
    for (float x : {-0.5f, 0.5f}) for (float y : {-0.5f, 0.5f}) {
        const glm::vec2 local = (glm::vec2(x, y) - pivot) * size;
        const glm::vec4 batched = mapModel * glm::vec4(center + local, 0.0f, 1.0f);
        const glm::vec4 perTile = tileModel * glm::vec4(local, 0.0f, 1.0f);
        EXPECT_NEAR(batched.x, perTile.x, 0.0001f);
        EXPECT_NEAR(batched.y, perTile.y, 0.0001f);
    }
    // The shader derives its tangent basis from uModel's first two columns.
    // Using the map model must retain the per-tile rotation/reflection.
    for (int axis = 0; axis < 2; ++axis) {
        const glm::vec2 batchAxis = glm::normalize(glm::vec2(mapModel[axis]));
        const glm::vec2 tileAxis = glm::normalize(glm::vec2(tileModel[axis]));
        EXPECT_NEAR(batchAxis.x, tileAxis.x, 0.00001f);
        EXPECT_NEAR(batchAxis.y, tileAxis.y, 0.00001f);
    }
}

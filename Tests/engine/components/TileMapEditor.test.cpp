#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <vector>
#include "RuntimeFixture.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/tilemap/TileRender.hpp"

class TileMapEditorTest : public RuntimeTest {
protected:
    std::string editorSceneId;

    GameObject* AddObject(GameObject* parent = nullptr) {
        auto* object = new GameObject();
        scene->AddGameObject(object);
        if (parent) object->GetTransform()->SetParent(parent->GetTransform(), false);
        return object;
    }

    TileMapEditor* AddTileMap(GameObject* object) {
        auto* editor = new TileMapEditor();
        object->AddComponent(editor);
        return editor;
    }

    TileMap* AddLoadedMap() {
        auto* map = new TileMap();
        map->SetID("test-cells-tilemap");
        map->SetFilePath((std::filesystem::path(PROJECT_ROOT) / "Tests" / "mocks" / "cells.tilemap").string());
        AssetManager::Get().AddTileMap(map);
        return map;
    }

    void TearDown() override {
        RuntimeTest::TearDown();
        if (!editorSceneId.empty()) {
            auto* editorContainer = Engine::Get()->GetEditorContainer();
            editorContainer->FindSystem<SceneManager>()->UnloadSceneNow(editorSceneId);
            editorContainer->FindSystem<Registry>()->FlushPendingShutdowns();
        }
        AssetManager::Get().RemoveAsset("test-cells-tilemap");
    }
};

TEST_F(TileMapEditorTest, AddingTileMapEditorCreatesRendererWithoutCreatingGrid) {
    auto* object = AddObject();
    auto* editor = AddTileMap(object);

    EXPECT_EQ(object->GetComponent<Grid>(), nullptr);
    EXPECT_EQ(editor->GetGrid(), nullptr);
    EXPECT_TRUE(editor->GetGridId().empty());
    EXPECT_NE(object->GetComponent<TileMapRenderer>(), nullptr);
}

TEST_F(TileMapEditorTest, NearestAncestorGridWinsWithoutChangingAuthoredSize) {
    auto* root = AddObject();
    root->AddComponent(new Grid());
    auto* parent = AddObject(root);
    auto* parentGrid = new Grid();
    parent->AddComponent(parentGrid);
    parentGrid->SetSize({8.0f, 12.0f});
    auto* object = AddObject(parent);
    auto* editor = AddTileMap(object);

    EXPECT_EQ(editor->GetGrid(), parentGrid);
    EXPECT_EQ(editor->GetGridId(), parentGrid->GetID());
    EXPECT_EQ(parentGrid->GetSize(), glm::vec2(8.0f, 12.0f));
    EXPECT_EQ(object->GetComponent<Grid>(), nullptr);

    std::unique_ptr<TileMapEditor> copy(editor->Copy());
    EXPECT_TRUE(copy->GetGridId().empty());
    EXPECT_FALSE(editor->Serialize()["gridId"].IsDefined());
}

TEST_F(TileMapEditorTest, LocalDescendantAndSiblingGridsAreIgnored) {
    auto* parent = AddObject();
    auto* object = AddObject(parent);
    object->AddComponent(new Grid());
    AddObject(object)->AddComponent(new Grid());
    AddObject(parent)->AddComponent(new Grid());
    auto* editor = AddTileMap(object);

    EXPECT_EQ(editor->GetGrid(), nullptr);
    EXPECT_TRUE(editor->GetGridId().empty());
    auto* parentGrid = new Grid();
    parent->AddComponent(parentGrid);
    EXPECT_EQ(editor->GetGrid(), parentGrid);
    parent->RemoveComponent(parentGrid);
    EXPECT_EQ(editor->GetGrid(), nullptr);
    EXPECT_TRUE(editor->GetGridId().empty());
}

TEST_F(TileMapEditorTest, GridRemovalFallsBackAndReplacementEmitsResolvedGridIds) {
    auto* root = AddObject();
    auto* rootGrid = new Grid();
    root->AddComponent(rootGrid);
    const std::string rootGridId = rootGrid->GetID();
    auto* parent = AddObject(root);
    auto* parentGrid = new Grid();
    parent->AddComponent(parentGrid);
    auto* editor = AddTileMap(AddObject(parent));
    auto changes = std::make_shared<std::vector<std::string>>();
    editor->Subscribe([changes](std::any data) {
        changes->push_back(std::any_cast<std::string>(data));
        return true;
    }, TileMapEditor::GRID_CHANGED_EVENT);

    parent->RemoveComponent(parentGrid);
    EXPECT_EQ(editor->GetGridId(), rootGridId);
    EXPECT_EQ(editor->GetGrid(), rootGrid);
    auto* replacementGrid = new Grid();
    parent->AddComponent(replacementGrid);
    const std::string replacementGridId = replacementGrid->GetID();
    EXPECT_EQ(editor->GetGridId(), replacementGridId);
    EXPECT_EQ(editor->GetGrid(), replacementGrid);
    parent->RemoveComponent(replacementGrid);
    root->RemoveComponent(rootGrid);
    EXPECT_EQ(editor->GetGrid(), nullptr);
    EXPECT_TRUE(editor->GetGridId().empty());
    EXPECT_EQ(*changes, (std::vector<std::string>{
        rootGridId, replacementGridId, rootGridId, ""}));
}

TEST_F(TileMapEditorTest, DestroyingGridThroughRegistryFallsBackToHigherAncestor) {
    auto* root = AddObject();
    auto* rootGrid = new Grid();
    root->AddComponent(rootGrid);
    auto* parent = AddObject(root);
    auto* parentGrid = new Grid();
    parent->AddComponent(parentGrid);
    auto* editor = AddTileMap(AddObject(parent));

    registry->Destroy(parentGrid);
    registry->FlushPendingShutdowns();
    EXPECT_EQ(editor->GetGridId(), rootGrid->GetID());
    EXPECT_EQ(editor->GetGrid(), rootGrid);
}

TEST_F(TileMapEditorTest, IntermediateReparentRebindsAndStopsObservingOldBranch) {
    auto* firstRoot = AddObject();
    auto* firstGrid = new Grid();
    firstRoot->AddComponent(firstGrid);
    auto* secondRoot = AddObject();
    auto* secondGrid = new Grid();
    secondRoot->AddComponent(secondGrid);
    auto* parent = AddObject(firstRoot);
    auto* editor = AddTileMap(AddObject(parent));
    auto changes = std::make_shared<std::vector<std::string>>();
    editor->Subscribe([changes](std::any data) {
        changes->push_back(std::any_cast<std::string>(data));
        return true;
    }, TileMapEditor::GRID_CHANGED_EVENT);

    parent->GetTransform()->SetParent(secondRoot->GetTransform(), false);
    EXPECT_EQ(editor->GetGridId(), secondGrid->GetID());
    EXPECT_EQ(editor->GetGrid(), secondGrid);
    firstRoot->RemoveComponent(firstGrid);
    firstRoot->AddComponent(new Grid());
    EXPECT_EQ(changes->size(), 1u);
    EXPECT_EQ(editor->GetGrid(), secondGrid);

    parent->GetTransform()->SetParent(nullptr, false);
    EXPECT_TRUE(editor->GetGridId().empty());
    EXPECT_EQ(editor->GetGrid(), nullptr);
    auto* lateGrid = new Grid();
    parent->AddComponent(lateGrid);
    EXPECT_EQ(editor->GetGridId(), lateGrid->GetID());
    EXPECT_EQ(editor->GetGrid(), lateGrid);
}

TEST_F(TileMapEditorTest, OwnerReparentFindsGridAddedAfterTileMapInitialization) {
    auto* object = AddObject();
    auto* editor = AddTileMap(object);
    auto* parent = AddObject();
    object->GetTransform()->SetParent(parent->GetTransform(), false);
    EXPECT_EQ(editor->GetGrid(), nullptr);

    auto* grid = new Grid();
    parent->AddComponent(grid);
    EXPECT_EQ(editor->GetGridId(), grid->GetID());
    EXPECT_EQ(editor->GetGrid(), grid);
    object->GetTransform()->SetParent(nullptr, false);
    EXPECT_EQ(editor->GetGrid(), nullptr);
    object->GetTransform()->SetParent(parent->GetTransform(), false);
    EXPECT_EQ(editor->GetGrid(), grid);
}

TEST_F(TileMapEditorTest, RemovingEditorRetiresAncestorSubscriptions) {
    auto* parent = AddObject();
    auto* grid = new Grid();
    parent->AddComponent(grid);
    auto* object = AddObject(parent);
    auto* editor = AddTileMap(object);
    const std::string editorId = editor->GetID();
    auto changes = std::make_shared<int>(0);
    editor->Subscribe([changes]() {
        ++*changes;
        return true;
    }, TileMapEditor::GRID_CHANGED_EVENT);

    object->RemoveComponent(editor);
    registry->FlushPendingShutdowns();
    EXPECT_EQ(registry->Find<TileMapEditor>(editorId), nullptr);
    const int notificationsAfterRemoval = *changes;
    parent->RemoveComponent(grid);
    parent->AddComponent(new Grid());
    object->GetTransform()->SetParent(nullptr, false);
    EXPECT_EQ(*changes, notificationsAfterRemoval);
}

TEST_F(TileMapEditorTest, RemovingAndReplacingGridKeepsTileMapLoaded) {
    auto* parent = AddObject();
    auto* originalGrid = new Grid();
    parent->AddComponent(originalGrid);
    auto* object = AddObject(parent);
    auto* editor = AddTileMap(object);
    auto* renderer = object->GetComponent<TileMapRenderer>();
    auto* map = AddLoadedMap();
    editor->SetTileMap(map);
    ASSERT_NE(map->GetCells(), nullptr);
    ASSERT_EQ(editor->GetTileId({1, 0}), 4);

    parent->RemoveComponent(originalGrid);
    registry->FlushPendingShutdowns();
    EXPECT_EQ(editor->GetGrid(), nullptr);
    EXPECT_EQ(object->GetComponent<TileMapEditor>(), editor);
    EXPECT_EQ(object->GetComponent<TileMapRenderer>(), renderer);
    EXPECT_NE(map->GetCells(), nullptr);
    EXPECT_EQ(editor->GetTileId({1, 0}), 4);

    auto* replacementGrid = new Grid();
    parent->AddComponent(replacementGrid);
    EXPECT_EQ(editor->GetGrid(), replacementGrid);
    EXPECT_EQ(renderer->GetGrid(), replacementGrid);
    EXPECT_EQ(editor->GetTileMap(), map);
}

TEST_F(TileMapEditorTest, CellCoordinatesAndRenderedGeometryUseTileMapTransform) {
    auto* parent = AddObject();
    parent->GetTransform()->SetPosition({40.0f, 20.0f});
    auto* grid = new Grid();
    parent->AddComponent(grid);
    grid->SetSize({8.0f, 12.0f});
    grid->SetGap({2.0f, 4.0f});
    auto* object = AddObject(parent);
    object->GetTransform()->SetPosition({10.0f, 5.0f});
    auto* editor = AddTileMap(object);
    auto* renderer = object->GetComponent<TileMapRenderer>();

    EXPECT_EQ(editor->CellToWorld({2, 1}), glm::vec2(70.0f, 41.0f));
    EXPECT_EQ(editor->WorldToCell({71.0f, 42.0f}), glm::ivec2(2, 1));
    EXPECT_EQ(editor->WorldToCell({49.0f, 24.0f}), glm::ivec2(-1, -1));
    const glm::mat4 model = TileRender::Model(grid, renderer, {2, 1}, {8.0f, 12.0f});
    EXPECT_FLOAT_EQ(model[3].x, 78.0f);
    EXPECT_FLOAT_EQ(model[3].y, 53.0f);

    object->GetTransform()->SetScale({2.0f, 3.0f});
    object->GetTransform()->SetRotation(90.0f);
    const glm::vec4 worldPoint = object->GetTransform()->GetWorldMatrix()
        * glm::vec4(21.0f, 17.0f, 0.0f, 1.0f);
    EXPECT_EQ(editor->WorldToCell(glm::vec2(worldPoint)), glm::ivec2(2, 1));
}

TEST_F(TileMapEditorTest, PlayModeCopyRebuildsGridBindingsInItsOwnWorld) {
    Engine::Get()->ExitPlayMode();
    auto* editorContainer = Engine::Get()->GetEditorContainer();
    auto* editorRegistry = editorContainer->FindSystem<Registry>();
    editorSceneId = "mock-scene";
    editorContainer->FindSystem<SceneManager>()->LoadScene(
        (std::filesystem::path(PROJECT_ROOT) / "Tests" / "mocks" / "mock.scene").string());
    scene = nullptr;
    while (!scene) {
        JobSystem::Get().Pump();
        scene = editorRegistry->Find<Scene>(editorSceneId);
    }
    auto* root = AddObject();
    auto* rootGrid = new Grid();
    root->AddComponent(rootGrid);
    auto* parent = AddObject(root);
    auto* editor = AddTileMap(AddObject(parent));
    const std::string editorId = editor->GetID();
    const std::string parentId = parent->GetID();
    const std::string rootGridId = rootGrid->GetID();
    auto changes = std::make_shared<int>(0);
    editor->Subscribe([changes]() {
        ++*changes;
        return true;
    }, TileMapEditor::GRID_CHANGED_EVENT);

    Engine::Get()->EnterPlayMode();
    registry = Engine::Get()->GetRuntimeContainer()->FindSystem<Registry>();
    auto* runtimeEditor = registry->Find<TileMapEditor>(editorId);
    auto* runtimeParent = registry->Find<GameObject>(parentId);
    ASSERT_NE(runtimeEditor, nullptr);
    ASSERT_NE(runtimeParent, nullptr);
    EXPECT_EQ(runtimeEditor->GetGrid(), registry->Find<Grid>(rootGridId));
    EXPECT_NE(runtimeEditor->GetGrid(), rootGrid);

    auto* runtimeGrid = new Grid();
    runtimeParent->AddComponent(runtimeGrid);
    EXPECT_EQ(runtimeEditor->GetGrid(), runtimeGrid);
    EXPECT_EQ(editor->GetGrid(), rootGrid);
    EXPECT_EQ(*changes, 0);

    auto* editorGrid = new Grid();
    parent->AddComponent(editorGrid);
    EXPECT_EQ(editor->GetGrid(), editorGrid);
    EXPECT_EQ(runtimeEditor->GetGrid(), runtimeGrid);
    EXPECT_EQ(*changes, 1);

    parent->RemoveComponent(editorGrid);
    EXPECT_EQ(editor->GetGrid(), rootGrid);
    EXPECT_EQ(runtimeEditor->GetGrid(), runtimeGrid);
    runtimeParent->RemoveComponent(runtimeGrid);
    EXPECT_EQ(runtimeEditor->GetGrid(), registry->Find<Grid>(rootGridId));
    root->RemoveComponent(rootGrid);
    EXPECT_EQ(editor->GetGrid(), nullptr);
    EXPECT_NE(registry->Find<Grid>(rootGridId), nullptr);
    EXPECT_EQ(runtimeEditor->GetGrid(), registry->Find<Grid>(rootGridId));
    EXPECT_EQ(*changes, 3);
}

TEST_F(TileMapEditorTest, EditingWithoutTileMapFailsWithoutChangingEmptyCells) {
    auto* editor = AddTileMap(AddObject());
    EXPECT_EQ(editor->GetTileMap(), nullptr);
    EXPECT_EQ(editor->GetTileId({1, 2}), EMPTY_TILE_ID);
    EXPECT_FALSE(editor->SetTileId({1, 2}, 7));
    EXPECT_FALSE(editor->ClearTile({1, 2}));
    for (TileId neighbor : editor->GetNeighbors({1, 2})) EXPECT_EQ(neighbor, EMPTY_TILE_ID);
}

TEST_F(TileMapEditorTest, LoadedTileMapEditsCellsAndReadsNeighbors) {
    auto* map = AddLoadedMap();
    auto* editor = AddTileMap(AddObject());
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

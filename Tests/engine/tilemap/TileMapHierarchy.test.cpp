#include <any>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "RuntimeFixture.hpp"
#include "engine/commands/ComponentCommand.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/TileMapEditor.hpp"
#include "engine/components/TileMapRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"

using TileMapHierarchyTest = RuntimeTest;

TEST_F(TileMapHierarchyTest, ChildInitializedBeforeAncestorResolvesDeserializedGrid) {
    const YAML::Node data = YAML::Load(R"(
id: tilemap-hierarchy-scene
gameobjects:
  - id: tilemap-child
    name: Tilemap
    component_ids: [tilemap-renderer, tilemap-transform]
  - id: grid-parent
    name: Grid
    component_ids: [grid-transform, parent-grid]
components:
  - id: tilemap-renderer
    type: TileMapRenderer
    gameobject: tilemap-child
  - id: tilemap-transform
    type: Transform
    gameobject: tilemap-child
    localPosition: [10, 5]
    localRotation: 0
    localScale: [1, 1]
    parent_id: grid-transform
  - id: grid-transform
    type: Transform
    gameobject: grid-parent
    localPosition: [40, 20]
    localRotation: 0
    localScale: [1, 1]
    parent_id: ~
  - id: parent-grid
    type: Grid
    gameobject: grid-parent
    cellSize: [8, 12]
    gapSize: [2, 4]
)");
    auto* loadedScene = new Scene();
    loadedScene->Attach(runtimeContainer);
    loadedScene->Deserialize(data);
    registry->Register(loadedScene);

    loadedScene->Init();

    auto* child = registry->Find<GameObject>("tilemap-child");
    auto* parent = registry->Find<GameObject>("grid-parent");
    auto* renderer = registry->Find<TileMapRenderer>("tilemap-renderer");
    auto* grid = registry->Find<Grid>("parent-grid");
    ASSERT_NE(child, nullptr);
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(renderer, nullptr);
    ASSERT_NE(grid, nullptr);
    auto* editor = child->GetComponent<TileMapEditor>();
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->GetGridId(), grid->GetID());
    EXPECT_EQ(renderer->GetGrid(), grid);

    loadedScene->PostInit();

    EXPECT_EQ(editor->GetGrid(), grid);
    EXPECT_EQ(child->GetComponent<Grid>(), nullptr);
    EXPECT_EQ(child->GetTransform()->GetParent(), parent->GetTransform());
    EXPECT_EQ(editor->CellToWorld({2, 1}), glm::vec2(70.0f, 41.0f));

    parent->RemoveComponent(grid);
    EXPECT_TRUE(editor->GetGridId().empty());
    auto* replacement = new Grid();
    parent->AddComponent(replacement);
    EXPECT_EQ(editor->GetGridId(), replacement->GetID());
    EXPECT_EQ(renderer->GetGrid(), replacement);
}

TEST_F(TileMapHierarchyTest, ComponentCommandUndoRestoresGridIdentityAndBinding) {
    auto* root = new GameObject();
    scene->AddGameObject(root);
    auto* rootGrid = new Grid();
    root->AddComponent(rootGrid);
    const std::string rootGridId = rootGrid->GetID();
    auto* parent = new GameObject();
    scene->AddGameObject(parent);
    parent->GetTransform()->SetParent(root->GetTransform(), false);
    auto* parentGrid = new Grid();
    parent->AddComponent(parentGrid);
    parentGrid->SetSize({16.0f, 24.0f});
    parentGrid->SetGap({2.0f, 3.0f});
    const std::string parentGridId = parentGrid->GetID();
    auto* child = new GameObject();
    scene->AddGameObject(child);
    child->GetTransform()->SetParent(parent->GetTransform(), false);
    auto* editor = new TileMapEditor();
    child->AddComponent(editor);
    auto* renderer = child->GetComponent<TileMapRenderer>();
    ASSERT_NE(renderer, nullptr);
    ASSERT_EQ(editor->GetGridId(), parentGridId);

    auto changes = std::make_shared<std::vector<std::string>>();
    editor->Subscribe([changes](const std::any& data) {
        changes->push_back(std::any_cast<std::string>(data));
        return true;
    }, TileMapEditor::GRID_CHANGED_EVENT);

    auto command = ComponentCommand::RemoveAndRecord(parent, parentGrid);
    ASSERT_NE(command, nullptr);
    registry->FlushPendingShutdowns();
    EXPECT_EQ(registry->Find<Grid>(parentGridId), nullptr);
    EXPECT_EQ(editor->GetGridId(), rootGridId);
    EXPECT_EQ(renderer->GetGrid(), rootGrid);

    command->Undo(runtimeContainer);
    auto* restoredGrid = registry->Find<Grid>(parentGridId);
    ASSERT_NE(restoredGrid, nullptr);
    EXPECT_EQ(restoredGrid->GetGameObject(), parent);
    EXPECT_EQ(restoredGrid->GetSize(), glm::vec2(16.0f, 24.0f));
    EXPECT_EQ(restoredGrid->GetGap(), glm::vec2(2.0f, 3.0f));
    EXPECT_EQ(editor->GetGridId(), parentGridId);
    EXPECT_EQ(renderer->GetGrid(), restoredGrid);

    restoredGrid->SetSize({20.0f, 30.0f});
    command->Redo(runtimeContainer);
    registry->FlushPendingShutdowns();
    EXPECT_EQ(editor->GetGridId(), rootGridId);
    EXPECT_EQ(renderer->GetGrid(), rootGrid);

    command->Undo(runtimeContainer);
    restoredGrid = registry->Find<Grid>(parentGridId);
    ASSERT_NE(restoredGrid, nullptr);
    EXPECT_EQ(restoredGrid->GetSize(), glm::vec2(20.0f, 30.0f));
    EXPECT_EQ(editor->GetGridId(), parentGridId);
    EXPECT_EQ(renderer->GetGrid(), restoredGrid);
    EXPECT_EQ(child->GetComponent<Grid>(), nullptr);
    EXPECT_EQ(*changes, (std::vector<std::string>{
        rootGridId, parentGridId, rootGridId, parentGridId}));
}

TEST_F(TileMapHierarchyTest, UndoBeforePendingShutdownFlushKeepsRestoredGridAlive) {
    auto* parent = new GameObject();
    scene->AddGameObject(parent);
    auto* grid = new Grid();
    parent->AddComponent(grid);
    grid->SetSize({16.0f, 24.0f});
    const std::string gridId = grid->GetID();
    auto* child = new GameObject();
    scene->AddGameObject(child);
    child->GetTransform()->SetParent(parent->GetTransform(), false);
    auto* editor = new TileMapEditor();
    child->AddComponent(editor);
    auto* renderer = child->GetComponent<TileMapRenderer>();
    ASSERT_NE(renderer, nullptr);

    auto changes = std::make_shared<std::vector<std::string>>();
    editor->Subscribe([changes](const std::any& data) {
        changes->push_back(std::any_cast<std::string>(data));
        return true;
    }, TileMapEditor::GRID_CHANGED_EVENT);

    auto command = ComponentCommand::RemoveAndRecord(parent, grid);
    ASSERT_NE(command, nullptr);
    EXPECT_EQ(registry->Find<Grid>(gridId), nullptr);
    EXPECT_EQ(editor->GetGrid(), nullptr);
    command->Undo(runtimeContainer);
    auto* restoredGrid = registry->Find<Grid>(gridId);
    ASSERT_NE(restoredGrid, nullptr);
    EXPECT_EQ(editor->GetGrid(), restoredGrid);

    registry->FlushPendingShutdowns();

    auto* liveGrid = registry->Find<Grid>(gridId);
    ASSERT_NE(liveGrid, nullptr);
    EXPECT_EQ(liveGrid, restoredGrid);
    EXPECT_EQ(parent->GetComponent<Grid>(), liveGrid);
    EXPECT_EQ(liveGrid->GetSize(), glm::vec2(16.0f, 24.0f));
    EXPECT_EQ(editor->GetGridId(), gridId);
    EXPECT_EQ(editor->GetGrid(), liveGrid);
    EXPECT_EQ(renderer->GetGrid(), liveGrid);
    EXPECT_EQ(*changes, (std::vector<std::string>{"", gridId}));
}

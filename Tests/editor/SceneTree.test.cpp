#include <QAbstractItemModelTester>
#include <QItemSelectionModel>
#include <gtest/gtest.h>

#include "EditorSceneFixture.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"
#include "utils/SceneTree.hpp"

class SceneTreeTest : public EditorSceneTest {};

TEST_F(SceneTreeTest, SceneTreeReflectsObjectAddRenameAndActiveState) {
    SceneTree tree;
    tree.RebuildFromScene(scene);
    QAbstractItemModelTester modelTester(
        tree.GetModel(), QAbstractItemModelTester::FailureReportingMode::Fatal);
    EXPECT_EQ(tree.GetModel()->rowCount(), 0);

    auto* object = new GameObject();
    object->SetName("Player");
    scene->AddGameObject(object);
    ASSERT_EQ(tree.GetModel()->rowCount(), 1);
    const QModelIndex item = tree.GetModel()->index(0, 0);
    ASSERT_TRUE(item.isValid());
    EXPECT_EQ(item.data(Qt::UserRole + 1).toString().toStdString(), object->GetID());
    EXPECT_EQ(item.data(Qt::DisplayRole).toString(), QStringLiteral("Player"));

    object->SetName("Hero");
    EXPECT_EQ(item.data(Qt::DisplayRole).toString(), QStringLiteral("Hero"));
    object->SetActive(false);
    EXPECT_FALSE(item.data(GameObjectItem::ActiveRole).toBool());
}

TEST_F(SceneTreeTest, ReparentingObjectMovesItsTreeRow) {
    SceneTree tree;
    tree.RebuildFromScene(scene);
    QAbstractItemModelTester modelTester(
        tree.GetModel(), QAbstractItemModelTester::FailureReportingMode::Fatal);
    auto* parent = new GameObject();
    parent->SetName("Parent");
    scene->AddGameObject(parent);
    auto* child = new GameObject();
    child->SetName("Child");
    scene->AddGameObject(child);
    ASSERT_EQ(tree.GetModel()->rowCount(), 2);

    child->GetTransform()->SetParent(parent->GetTransform(), false);
    ASSERT_EQ(tree.GetModel()->rowCount(), 1);
    const QModelIndex parentItem = tree.GetModel()->index(0, 0);
    ASSERT_EQ(tree.GetModel()->rowCount(parentItem), 1);
    EXPECT_EQ(tree.GetModel()->index(0, 0, parentItem).data(Qt::UserRole + 1).toString().toStdString(),
              child->GetID());
}

TEST_F(SceneTreeTest, EngineSelectionHighlightsMatchingTreeRow) {
    SceneTree tree;
    tree.RebuildFromScene(scene);
    QAbstractItemModelTester modelTester(
        tree.GetModel(), QAbstractItemModelTester::FailureReportingMode::Fatal);
    auto* object = new GameObject();
    scene->AddGameObject(object);
    const QModelIndex item = tree.GetModel()->index(0, 0);
    ASSERT_TRUE(item.isValid());
    auto* selection = editorContainer->FindSystem<SelectionManager>();
    ASSERT_NE(selection, nullptr);

    selection->Select(object->GetID());
    EXPECT_TRUE(tree.selectionModel()->isSelected(item));
    selection->ClearSelection();
    EXPECT_FALSE(tree.selectionModel()->isSelected(item));
}

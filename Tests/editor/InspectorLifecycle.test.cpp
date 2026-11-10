#include <QTest>
#include <gtest/gtest.h>

#include "EditorSceneFixture.hpp"
#include "dock-widgets/InspectorGui.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/SelectionManager.hpp"

class InspectorLifecycleTest : public EditorSceneTest {};

TEST_F(InspectorLifecycleTest, SelectedObjectSceneUnloadDoesNotUseFreedSubscriptions) {
    auto* inspector = InspectorGui::Get();
    inspector->Init();

    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* selection = editorContainer->FindSystem<SelectionManager>();
    ASSERT_NE(selection, nullptr);
    selection->Select(object->GetID());
    QTRY_VERIFY_WITH_TIMEOUT(!inspector->findChildren<ObjectHeader*>().empty(), 500);

    const std::string sceneId = scene->GetID();
    sceneManager->UnloadSceneNow(sceneId);
    scene = nullptr;
    registry->FlushPendingShutdowns();

    EXPECT_TRUE(selection->GetSelectedId().empty());
    QTRY_VERIFY_WITH_TIMEOUT(inspector->findChildren<ObjectHeader*>().empty(), 500);
}

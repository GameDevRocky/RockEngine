#include <algorithm>
#include <memory>

#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/Grid.hpp"





class GameObjectTest : public RuntimeTest {
protected:
    GameObject* object = nullptr;

    void SetUp() override {
        RuntimeTest::SetUp();
        if (HasFatalFailure()) return;

        object = new GameObject();
        scene->AddGameObject(object);
    }
};

class LifecycleProbe : public Component {
public:
    int awakeCalls = 0;
    int startCalls = 0;
    int updateCalls = 0;
    void Awake() override { ++awakeCalls; }
    void Start() override { ++startCalls; }
    void Update() override { ++updateCalls; }
    std::string GetTypeName() const override { return "LifecycleProbe"; }
};


TEST_F(GameObjectTest, AddedObjectBelongsToRuntimeScene) {
    EXPECT_EQ(object->GetContainer(), runtimeContainer);
    EXPECT_EQ(object->GetScene(), scene);
    EXPECT_EQ(registry->Find<GameObject>(object->GetID()), object);

    const auto& objects = scene->GetAllGameObjects();
    EXPECT_NE(std::find(objects.begin(), objects.end(), object), objects.end());
}

TEST_F(GameObjectTest, AddedObjectHasTransform) {
    Transform* transform = object->GetTransform();
    ASSERT_NE(transform, nullptr);
    EXPECT_EQ(transform->GetGameObject(), object);
    EXPECT_EQ(object->GetComponent<Transform>(), transform);
}

TEST_F(GameObjectTest, RenameObject) {
    object->SetName("Test Object");
    EXPECT_EQ(object->GetName(), "Test Object");
}

TEST_F(GameObjectTest, CopyDoesNotCarrySourceEventSubscriptions) {
    int sourceNotifications = 0;
    const int subscriptionId = object->Subscribe([&sourceNotifications]() {
        ++sourceNotifications;
        return true;
    }, GameObject::NAME_CHANGED_EVENT);

    std::unique_ptr<GameObject> copy(object->Copy());
    copy->SetName("Copied Object");
    EXPECT_EQ(sourceNotifications, 0);

    object->SetName("Original Object");
    EXPECT_EQ(sourceNotifications, 1);
    object->Unsubscribe(subscriptionId);
}

TEST_F(GameObjectTest, ActiveStateControlsComponents) {
    Transform* transform = object->GetTransform();
    ASSERT_NE(transform, nullptr);

    object->SetActive(false);
    EXPECT_FALSE(object->GetActive());
    EXPECT_FALSE(transform->GetEnabled());

    object->SetActive(true);
    EXPECT_TRUE(object->GetActive());
    EXPECT_TRUE(transform->GetEnabled());
}

TEST_F(GameObjectTest, RuntimeComponentReceivesLifecycleAndDisabledComponentSkipsUpdate) {
    auto* probe = new LifecycleProbe();
    object->AddComponent(probe);
    EXPECT_EQ(probe->awakeCalls, 1);
    EXPECT_EQ(probe->startCalls, 1);
    object->Update();
    EXPECT_EQ(probe->updateCalls, 1);
    probe->SetEnabled(false);
    object->Update();
    EXPECT_EQ(probe->updateCalls, 1);
    probe->SetEnabled(true);
    object->Update();
    EXPECT_EQ(probe->updateCalls, 2);
}

TEST_F(GameObjectTest, ComponentOrderChangesAreReflectedInLookupAndSerialization) {
    auto* first = new LifecycleProbe();
    auto* second = new LifecycleProbe();
    object->AddComponent(first);
    object->AddComponent(second);
    EXPECT_EQ(object->GetComponent<LifecycleProbe>(), first);
    EXPECT_TRUE(object->MoveComponent(second->GetID(), 1));
    EXPECT_EQ(object->GetComponent<LifecycleProbe>(), second);
    EXPECT_EQ(object->GetComponentIndex(second->GetID()), 1);
    EXPECT_FALSE(object->MoveComponent("missing", 0));
    EXPECT_FALSE(object->MoveComponent(second->GetID(), 1));
    EXPECT_EQ(object->Serialize()["component_ids"][1].as<std::string>(), second->GetID());
}

TEST_F(GameObjectTest, DeactivatingParentPropagatesToChild) {
    auto* child = new GameObject();
    scene->AddGameObject(child);
    child->GetTransform()->SetParent(object->GetTransform(), false);
    object->SetActive(false);
    EXPECT_FALSE(object->GetActive());
    EXPECT_FALSE(child->GetActive());
    object->SetActive(true);
    EXPECT_TRUE(child->GetActive());
}

TEST_F(GameObjectTest, SingletonComponentsRejectDuplicatesButOtherComponentsCanRepeat) {
    auto* firstGrid = new Grid();
    object->AddComponent(firstGrid);
    object->AddComponent(new Grid());
    EXPECT_EQ(object->GetComponents<Grid>().size(), 1u);
    EXPECT_EQ(object->GetComponent<Grid>(), firstGrid);

    object->AddComponent(new LifecycleProbe());
    object->AddComponent(new LifecycleProbe());
    EXPECT_EQ(object->GetComponents<LifecycleProbe>().size(), 2u);
}

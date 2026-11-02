#include <any>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "RuntimeFixture.hpp"
#include "engine/components/Component.hpp"
#include "engine/components/ComponentImpl.hpp"
#include "engine/components/Grid.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/GameObjectImpl.hpp"

namespace {

class ProbeComponent : public Component {
public:
    int enabledCalls = 0;
    int disabledCalls = 0;

    std::string GetTypeName() const override { return "ProbeComponent"; }
    void OnEnabled() override { ++enabledCalls; }
    void OnDisabled() override { ++disabledCalls; }
};

class ComponentTest : public RuntimeTest {
protected:
    GameObject* object = nullptr;
    ProbeComponent* component = nullptr;

    void SetUp() override {
        RuntimeTest::SetUp();
        if (HasFatalFailure()) return;

        object = new GameObject();
        scene->AddGameObject(object);

        component = new ProbeComponent();
        object->AddComponent(component);
    }
};

TEST_F(ComponentTest, AttachedComponentResolvesItsOwnerAndTransform) {
    EXPECT_EQ(component->GetContainer(), runtimeContainer);
    EXPECT_EQ(component->GetGameObject(), object);
    EXPECT_EQ(component->GetTransform(), object->GetTransform());
    EXPECT_EQ(registry->Find<ProbeComponent>(component->GetID()), component);
}

TEST_F(ComponentTest, EnabledStateNotifiesOnlyWhenItChanges) {
    std::vector<bool> notifications;
    component->Subscribe([&notifications](std::any payload) {
        if (const auto* enabled = std::any_cast<bool>(&payload)) {
            notifications.push_back(*enabled);
        }
        return true;
    }, Component::ENABLED_CHANGED_EVENT);

    component->SetEnabled(false);
    component->SetEnabled(false);
    component->SetEnabled(true);

    EXPECT_TRUE(component->GetEnabled());
    EXPECT_EQ(component->disabledCalls, 1);
    EXPECT_EQ(component->enabledCalls, 1);
    EXPECT_EQ(notifications, (std::vector<bool>{false, true}));
}

TEST_F(ComponentTest, RequireComponentCreatesMissingComponentAndReusesExistingOne) {
    Transform* transform = object->GetTransform();
    ASSERT_NE(transform, nullptr);
    EXPECT_EQ(component->GetComponent<Transform>(), transform);
    EXPECT_EQ(component->RequireComponent<Transform>(), transform);

    Grid* grid = component->RequireComponent<Grid>();
    ASSERT_NE(grid, nullptr);
    EXPECT_EQ(grid->GetGameObject(), object);
    EXPECT_EQ(component->GetComponent<Grid>(), grid);
    EXPECT_EQ(component->RequireComponent<Grid>(), grid);
}

TEST_F(ComponentTest, SerializationPreservesOwnerAndEnabledState) {
    component->SetEnabled(false);
    YAML::Node saved = component->Serialize();

    EXPECT_EQ(saved["type"].as<std::string>(), "ProbeComponent");
    EXPECT_EQ(saved["gameobject"].as<std::string>(), object->GetID());
    EXPECT_FALSE(saved["enabled"].as<bool>());

    ProbeComponent restored;
    restored.Deserialize(saved);
    restored.Attach(runtimeContainer);
    EXPECT_EQ(restored.GetID(), component->GetID());
    EXPECT_EQ(restored.GetGameObject(), object);
    EXPECT_FALSE(restored.GetEnabled());
}

TEST_F(ComponentTest, DeserializationDefaultsMissingOptionalFields) {
    ProbeComponent restored;
    YAML::Node saved;
    saved["id"] = "legacy-component";

    restored.Deserialize(saved);

    EXPECT_EQ(restored.GetID(), "legacy-component");
    EXPECT_TRUE(restored.GetEnabled());
    EXPECT_EQ(restored.GetGameObject(), nullptr);
}

TEST_F(ComponentTest, CopyToContainerKeepsComponentData) {
    Grid* grid = component->RequireComponent<Grid>();
    ASSERT_NE(grid, nullptr);
    grid->SetSize({8.0f, 16.0f});

    std::unique_ptr<Component> copied(static_cast<Component*>(grid)->Copy(runtimeContainer));
    auto* copiedGrid = dynamic_cast<Grid*>(copied.get());
    ASSERT_NE(copiedGrid, nullptr);
    EXPECT_EQ(copiedGrid->GetContainer(), runtimeContainer);
    EXPECT_EQ(copiedGrid->GetID(), grid->GetID());
    EXPECT_EQ(copiedGrid->GetGameObject(), object);
    EXPECT_EQ(copiedGrid->GetSize(), grid->GetSize());
}

TEST_F(ComponentTest, ParentLookupFindsNearestMatchingComponent) {
    auto* child = new GameObject();
    scene->AddGameObject(child);
    child->GetTransform()->SetParent(object->GetTransform(), false);
    auto* grandchild = new GameObject();
    scene->AddGameObject(grandchild);
    grandchild->GetTransform()->SetParent(child->GetTransform(), false);
    auto* childProbe = new ProbeComponent();
    child->AddComponent(childProbe);
    auto* grandchildProbe = new ProbeComponent();
    grandchild->AddComponent(grandchildProbe);

    EXPECT_EQ(grandchildProbe->GetComponentInParent<ProbeComponent>(), childProbe);
    EXPECT_EQ(childProbe->GetComponentInParent<ProbeComponent>(), component);
    EXPECT_EQ(component->GetComponentInParent<ProbeComponent>(), nullptr);
}

TEST_F(ComponentTest, RemovalImmediatelyDisappearsFromObjectLookup) {
    const std::string id = component->GetID();
    object->RemoveComponent(component);
    EXPECT_EQ(object->GetComponent<ProbeComponent>(), nullptr);
    EXPECT_FALSE(object->HasComponentByName("ProbeComponent"));
    EXPECT_EQ(object->GetComponentIndex(id), -1);
}

} // namespace

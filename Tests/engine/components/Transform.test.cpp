#include <algorithm>
#include <memory>

#include <gtest/gtest.h>

#include "RuntimeFixture.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

namespace {

class TransformTest : public RuntimeTest {
protected:
    GameObject* object = nullptr;
    Transform* transform = nullptr;

    GameObject* AddObject() {
        auto* added = new GameObject();
        scene->AddGameObject(added);
        return added;
    }

    void SetUp() override {
        RuntimeTest::SetUp();
        if (HasFatalFailure()) return;

        object = AddObject();
        transform = object->GetTransform();
        ASSERT_NE(transform, nullptr);
    }
};

TEST_F(TransformTest, NewObjectHasIdentityTransform) {
    EXPECT_FLOAT_EQ(transform->GetWorldPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(transform->GetWorldPosition().y, 0.0f);
    EXPECT_FLOAT_EQ(transform->GetWorldRotation(), 0.0f);
    EXPECT_FLOAT_EQ(transform->GetWorldScale().x, 1.0f);
    EXPECT_FLOAT_EQ(transform->GetWorldScale().y, 1.0f);
    EXPECT_EQ(transform->GetParent(), nullptr);
}

TEST_F(TransformTest, LocalSettersUpdateRootWorldTransform) {
    transform->SetPosition({3.0f, 4.0f});
    transform->SetRotation(45.0f);
    transform->SetScale({2.0f, 3.0f});

    EXPECT_NEAR(transform->GetWorldPosition().x, 3.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldPosition().y, 4.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldRotation(), 45.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldScale().x, 2.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldScale().y, 3.0f, 0.0001f);
}

TEST_F(TransformTest, ChildWorldPositionIncludesParentRotationAndScale) {
    GameObject* child = AddObject();
    Transform* childTransform = child->GetTransform();
    ASSERT_NE(childTransform, nullptr);

    transform->SetPosition({10.0f, 20.0f});
    transform->SetRotation(90.0f);
    transform->SetScale({2.0f, 2.0f});
    childTransform->SetPosition({3.0f, 4.0f});
    childTransform->SetParent(transform, false);

    EXPECT_EQ(childTransform->GetParent(), transform);
    EXPECT_NEAR(childTransform->GetWorldPosition().x, 2.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldPosition().y, 26.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldRotation(), 90.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldScale().x, 2.0f, 0.0001f);
}

TEST_F(TransformTest, ReparentingPreservesWorldPoseByDefault) {
    GameObject* parent = AddObject();
    Transform* parentTransform = parent->GetTransform();
    ASSERT_NE(parentTransform, nullptr);

    transform->SetPosition({3.0f, 4.0f});
    transform->SetRotation(15.0f);
    transform->SetScale({2.0f, 3.0f});
    parentTransform->SetPosition({10.0f, 20.0f});

    transform->SetParent(parentTransform);

    EXPECT_EQ(transform->GetParent(), parentTransform);
    EXPECT_NEAR(transform->GetWorldPosition().x, 3.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldPosition().y, 4.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldRotation(), 15.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldScale().x, 2.0f, 0.0001f);
    EXPECT_NEAR(transform->GetWorldScale().y, 3.0f, 0.0001f);

    const auto& children = parentTransform->GetChildren();
    EXPECT_NE(std::find(children.begin(), children.end(), transform), children.end());
}

TEST_F(TransformTest, MovingParentRefreshesChildWorldMatrix) {
    GameObject* child = AddObject();
    Transform* childTransform = child->GetTransform();
    ASSERT_NE(childTransform, nullptr);

    childTransform->SetPosition({2.0f, 3.0f});
    childTransform->SetParent(transform, false);
    EXPECT_NEAR(childTransform->GetWorldPosition().x, 2.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldPosition().y, 3.0f, 0.0001f);

    transform->SetPosition({10.0f, 20.0f});

    EXPECT_NEAR(childTransform->GetWorldPosition().x, 12.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldPosition().y, 23.0f, 0.0001f);
}

TEST_F(TransformTest, WorldSettersAccountForParentTransform) {
    GameObject* child = AddObject();
    Transform* childTransform = child->GetTransform();
    ASSERT_NE(childTransform, nullptr);

    transform->SetPosition({10.0f, 20.0f});
    transform->SetRotation(30.0f);
    transform->SetScale({2.0f, 2.0f});
    childTransform->SetParent(transform, false);

    childTransform->SetWorldPosition({14.0f, 26.0f});
    childTransform->SetWorldRotation(90.0f);
    childTransform->SetWorldScale({4.0f, 6.0f});

    EXPECT_NEAR(childTransform->GetWorldPosition().x, 14.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldPosition().y, 26.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldRotation(), 90.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldScale().x, 4.0f, 0.0001f);
    EXPECT_NEAR(childTransform->GetWorldScale().y, 6.0f, 0.0001f);
}

TEST_F(TransformTest, SerializationRestoresLocalValues) {
    transform->SetPosition({5.0f, -7.0f});
    transform->SetRotation(25.0f);
    transform->SetScale({2.0f, 3.0f});

    YAML::Node saved = transform->Serialize();
    Transform restored;
    restored.Deserialize(saved);

    EXPECT_EQ(restored.GetID(), transform->GetID());
    EXPECT_FLOAT_EQ(restored.localPosition.x, 5.0f);
    EXPECT_FLOAT_EQ(restored.localPosition.y, -7.0f);
    EXPECT_FLOAT_EQ(restored.localRotation, 25.0f);
    EXPECT_FLOAT_EQ(restored.localScale.x, 2.0f);
    EXPECT_FLOAT_EQ(restored.localScale.y, 3.0f);
    EXPECT_TRUE(restored.Serialize()["parent_id"].IsNull());
}

TEST_F(TransformTest, CopyPreservesAuthoredValues) {
    transform->SetPosition({5.0f, -7.0f});
    transform->SetRotation(25.0f);
    transform->SetScale({2.0f, 3.0f});
    transform->SetEnabled(false);

    std::unique_ptr<Transform> copied(transform->Copy());

    ASSERT_NE(copied, nullptr);
    EXPECT_EQ(copied->GetID(), transform->GetID());
    EXPECT_FLOAT_EQ(copied->localPosition.x, transform->localPosition.x);
    EXPECT_FLOAT_EQ(copied->localPosition.y, transform->localPosition.y);
    EXPECT_FLOAT_EQ(copied->localRotation, transform->localRotation);
    EXPECT_FLOAT_EQ(copied->localScale.x, transform->localScale.x);
    EXPECT_FLOAT_EQ(copied->localScale.y, transform->localScale.y);
    EXPECT_FALSE(copied->GetEnabled());
}

TEST_F(TransformTest, DetachingChildPreservesWorldPoseAndUpdatesParentCache) {
    GameObject* child = AddObject();
    Transform* childTransform = child->GetTransform();
    transform->SetPosition({10.0f, 20.0f});
    childTransform->SetPosition({2.0f, 3.0f});
    childTransform->SetParent(transform, false);
    ASSERT_EQ(transform->GetChildren().size(), 1u);
    const glm::vec2 before = childTransform->GetWorldPosition();

    childTransform->SetParent(nullptr);
    EXPECT_EQ(childTransform->GetParent(), nullptr);
    EXPECT_TRUE(transform->GetChildren().empty());
    EXPECT_NEAR(childTransform->GetWorldPosition().x, before.x, 0.001f);
    EXPECT_NEAR(childTransform->GetWorldPosition().y, before.y, 0.001f);
}

TEST_F(TransformTest, MovingChildUpdatesResolvedHierarchyOrder) {
    GameObject* first = AddObject();
    GameObject* second = AddObject();
    first->GetTransform()->SetParent(transform, false);
    second->GetTransform()->SetParent(transform, false);
    ASSERT_EQ(transform->GetChildren().size(), 2u);
    transform->MoveChild(second->GetTransform()->GetID(), 0);
    EXPECT_EQ(transform->GetChildren()[0], second->GetTransform());
    EXPECT_EQ(transform->GetChildren()[1], first->GetTransform());
    transform->MoveChild("missing", 0);
    EXPECT_EQ(transform->GetChildren()[0], second->GetTransform());
}

TEST_F(TransformTest, LocalSettersNotifyOnlyForChangedValues) {
    int positionEvents = 0;
    transform->Subscribe([&positionEvents]() { ++positionEvents; return true; }, Transform::POSITION_CHANGED_EVENT);
    transform->SetPosition({0.0f, 0.0f});
    transform->SetPosition({2.0f, 3.0f});
    transform->SetPosition({2.0f, 3.0f});
    EXPECT_EQ(positionEvents, 1);
}

} // namespace

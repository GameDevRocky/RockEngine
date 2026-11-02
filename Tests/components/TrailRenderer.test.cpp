#include <memory>
#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/TrailRenderer.hpp"
#include "engine/components/Transform.hpp"
#include "engine/core/GameObject.hpp"

using TrailRendererTest = RuntimeTest;

TEST_F(TrailRendererTest, RecordsOnlyMovementBeyondMinimumDistanceAndCopyStartsEmpty) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* trail = new TrailRenderer();
    object->AddComponent(trail);
    trail->SetMinVertexDistance(5.0f);

    trail->LateUpdate();
    ASSERT_EQ(trail->GetPoints().size(), 1u);
    object->GetTransform()->SetPosition({3.0f, 0.0f});
    trail->LateUpdate();
    EXPECT_EQ(trail->GetPoints().size(), 1u);
    object->GetTransform()->SetPosition({6.0f, 0.0f});
    trail->LateUpdate();
    EXPECT_EQ(trail->GetPoints().size(), 2u);

    std::unique_ptr<TrailRenderer> copy(trail->Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_TRUE(copy->GetPoints().empty());
}

TEST_F(TrailRendererTest, TurningOffEmissionPreservesHistoryUntilCleared) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* trail = new TrailRenderer();
    object->AddComponent(trail);
    trail->LateUpdate();
    const auto revision = trail->GetRevision();
    ASSERT_EQ(trail->GetPoints().size(), 1u);
    trail->SetEmitting(false);
    object->GetTransform()->SetPosition({100.0f, 0.0f});
    trail->LateUpdate();
    EXPECT_EQ(trail->GetPoints().size(), 1u);
    EXPECT_EQ(trail->GetRevision(), revision);
    trail->Clear();
    EXPECT_TRUE(trail->GetPoints().empty());
    EXPECT_GT(trail->GetRevision(), revision);
    const auto clearedRevision = trail->GetRevision();
    trail->Clear();
    EXPECT_EQ(trail->GetRevision(), clearedRevision);
}

TEST_F(TrailRendererTest, CopyAndSerializationKeepSettingsWithoutHistory) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* trail = new TrailRenderer();
    object->AddComponent(trail);
    trail->SetTime(-1.0f);
    trail->SetMinVertexDistance(-5.0f);
    EXPECT_FLOAT_EQ(trail->GetTime(), 0.0f);
    EXPECT_FLOAT_EQ(trail->GetMinVertexDistance(), 0.0f);
    trail->SetTime(3.0f);
    trail->SetSortingLayer("Effects");
    trail->SetEmitting(false);
    trail->LateUpdate();

    std::unique_ptr<TrailRenderer> copy(trail->Copy());
    TrailRenderer restored;
    restored.Deserialize(trail->Serialize());
    for (const TrailRenderer* result : {copy.get(), &restored}) {
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->GetTime(), 3.0f);
        EXPECT_EQ(result->GetSortingLayer(), "Effects");
        EXPECT_FALSE(result->GetEmitting());
        EXPECT_TRUE(result->GetPoints().empty());
        EXPECT_EQ(result->GetRevision(), 0u);
    }
}

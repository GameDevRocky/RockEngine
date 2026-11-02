#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/Animator.hpp"
#include "engine/core/GameObject.hpp"

using AnimatorTest = RuntimeTest;

TEST_F(AnimatorTest, AwakeEntersDefaultStateAndPlaySwitchesState) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* animator = new Animator();
    animator->AddState("Idle");
    animator->AddState("Run");
    animator->SetDefaultState("Idle");
    object->AddComponent(animator);

    EXPECT_EQ(animator->GetCurrentState(), "Idle");
    animator->Play("Run");
    EXPECT_EQ(animator->GetCurrentState(), "Run");
}

TEST_F(AnimatorTest, TransitionChecksEveryConditionAndConsumesTrigger) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* animator = new Animator();
    animator->AddState("Idle");
    animator->AddState("Run");
    animator->AddParameter("speed", AnimatorParameter::Type::Float);
    animator->AddParameter("go", AnimatorParameter::Type::Trigger);
    auto* transition = animator->AddTransition("Idle", "Run");
    transition->conditions.push_back({"speed", AnimatorCondition::Mode::Greater, 2.0f});
    transition->conditions.push_back({"go", AnimatorCondition::Mode::If, 0.0f});
    object->AddComponent(animator);

    animator->SetFloat("speed", 3.0f);
    animator->Update();
    EXPECT_EQ(animator->GetCurrentState(), "Idle");
    animator->SetTrigger("go");
    animator->Update();
    EXPECT_EQ(animator->GetCurrentState(), "Run");
    EXPECT_FALSE(animator->GetBool("go"));
}

TEST_F(AnimatorTest, EditingGraphMaintainsReferencesAndRejectsInvalidRenames) {
    Animator animator;
    animator.AddState("Idle");
    animator.AddState("Run");
    EXPECT_EQ(animator.AddState("Run")->name, "Run 1");
    auto* transition = animator.AddTransition("Idle", "Run");
    const std::string transitionId = transition->id;
    animator.AddParameter("speed", AnimatorParameter::Type::Float);
    animator.FindTransition(transitionId)->conditions.push_back({"speed", AnimatorCondition::Mode::Greater, 1.0f});

    animator.RenameState("Idle", "Run");
    EXPECT_NE(animator.FindState("Idle"), nullptr);
    animator.RenameState("Idle", "Rest");
    EXPECT_EQ(animator.GetDefaultState(), "Rest");
    EXPECT_EQ(animator.FindTransition(transitionId)->fromState, "Rest");
    animator.RenameParameter("speed", "velocity");
    EXPECT_EQ(animator.FindTransition(transitionId)->conditions.front().parameter, "velocity");
    animator.RemoveParameter("velocity");
    EXPECT_TRUE(animator.FindTransition(transitionId)->conditions.empty());
    animator.RemoveState("Run");
    EXPECT_EQ(animator.FindTransition(transitionId), nullptr);
}

TEST_F(AnimatorTest, FrameEditingPreservesOrderAndIgnoresInvalidIndices) {
    Animator animator;
    animator.AddState("Idle");
    animator.AddFrame("Idle", "a");
    animator.AddFrame("Idle", "b");
    animator.AddFrame("Idle", "c");
    animator.MoveFrame("Idle", 0, 2);
    EXPECT_EQ(animator.FindState("Idle")->frames, (std::vector<std::string>{"b", "c", "a"}));
    animator.MoveFrame("Idle", -1, 0);
    animator.SetFrame("Idle", 1, "");
    animator.RemoveFrame("Idle", 99);
    EXPECT_EQ(animator.FindState("Idle")->frames, (std::vector<std::string>{"b", "", "a"}));
}

TEST_F(AnimatorTest, SerializationPersistsGraphButResetsParameterRuntimeValue) {
    Animator animator;
    animator.AddState("Idle");
    animator.AddState("Run");
    auto* speed = animator.AddParameter("speed", AnimatorParameter::Type::Float);
    speed->defaultValue = 1.5f;
    animator.SetFloat("speed", 8.0f);
    animator.AddTransition("Idle", "Run")->conditions.push_back({"speed", AnimatorCondition::Mode::Greater, 2.0f});

    Animator restored;
    restored.Deserialize(animator.Serialize());
    ASSERT_EQ(restored.GetStates().size(), 2u);
    ASSERT_EQ(restored.GetTransitions().size(), 1u);
    EXPECT_EQ(restored.GetDefaultState(), "Idle");
    EXPECT_EQ(restored.GetFloat("speed"), 1.5f);
    EXPECT_EQ(restored.GetTransitions().front().conditions.front().parameter, "speed");
}

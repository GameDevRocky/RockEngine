#include <gtest/gtest.h>
#include <memory>
#include "RuntimeFixture.hpp"
#include "engine/components/AudioListener.hpp"
#include "engine/core/GameObject.hpp"

using AudioListenerTest = RuntimeTest;

TEST_F(AudioListenerTest, MainListenerIgnoresDisabledObject) {
    auto* object = new GameObject();
    scene->AddGameObject(object);
    auto* listener = new AudioListener();
    object->AddComponent(listener);

    EXPECT_EQ(AudioListener::GetMain(), listener);
    object->SetActive(false);
    EXPECT_EQ(AudioListener::GetMain(), nullptr);
}

TEST_F(AudioListenerTest, DisabledListenerIsSkippedAndCopyKeepsIdentity) {
    auto* firstObject = new GameObject();
    scene->AddGameObject(firstObject);
    auto* first = new AudioListener();
    firstObject->AddComponent(first);
    auto* secondObject = new GameObject();
    scene->AddGameObject(secondObject);
    auto* second = new AudioListener();
    secondObject->AddComponent(second);

    first->SetEnabled(false);
    EXPECT_EQ(AudioListener::GetMain(), second);
    second->SetEnabled(false);
    EXPECT_EQ(AudioListener::GetMain(), nullptr);
    std::unique_ptr<AudioListener> copy(first->Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetID(), first->GetID());
    EXPECT_FALSE(copy->GetEnabled());
}

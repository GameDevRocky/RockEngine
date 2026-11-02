#include <memory>
#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/AudioSource.hpp"

using AudioSourceTest = RuntimeTest;

TEST_F(AudioSourceTest, VolumeAndSpatialBlendAreClampedAndCopied) {
    AudioSource source;
    source.SetVolume(2.0f);
    source.SetSpatialBlend(-1.0f);
    EXPECT_FLOAT_EQ(source.GetVolume(), 1.0f);
    EXPECT_FLOAT_EQ(source.GetSpatialBlend(), 0.0f);

    source.SetVolume(0.25f);
    source.SetSpatialBlend(0.75f);
    std::unique_ptr<AudioSource> copy(source.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_FLOAT_EQ(copy->GetVolume(), 0.25f);
    EXPECT_FLOAT_EQ(copy->GetSpatialBlend(), 0.75f);
}

TEST_F(AudioSourceTest, MissingClipPlaybackControlsAreSafeNoOps) {
    AudioSource source;
    source.SetPlayOnAwake(false);
    source.Play();
    EXPECT_FALSE(source.IsPlaying());
    source.Pause();
    source.UnPause();
    source.Stop();
    source.PlayOneShot();
    EXPECT_FALSE(source.IsPlaying());
}

TEST_F(AudioSourceTest, SerializationAndCopyKeepPlaybackConfiguration) {
    AudioSource source;
    source.SetClip("missing-clip");
    source.SetVolume(0.3f);
    source.SetPitch(1.25f);
    source.SetLoop(true);
    source.SetMute(true);
    source.SetPlayOnAwake(false);
    source.SetSpatialBlend(0.8f);
    source.SetMinDistance(25.0f);
    source.SetMaxDistance(300.0f);

    std::unique_ptr<AudioSource> copy(source.Copy());
    AudioSource restored;
    restored.Deserialize(source.Serialize());
    for (const AudioSource* result : {copy.get(), &restored}) {
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->GetClipID(), "missing-clip");
        EXPECT_FLOAT_EQ(result->GetVolume(), 0.3f);
        EXPECT_FLOAT_EQ(result->GetPitch(), 1.25f);
        EXPECT_TRUE(result->GetLoop());
        EXPECT_TRUE(result->GetMute());
        EXPECT_FALSE(result->GetPlayOnAwake());
        EXPECT_FLOAT_EQ(result->GetSpatialBlend(), 0.8f);
        EXPECT_FLOAT_EQ(result->GetMinDistance(), 25.0f);
        EXPECT_FLOAT_EQ(result->GetMaxDistance(), 300.0f);
        EXPECT_FALSE(result->IsPlaying());
    }
}

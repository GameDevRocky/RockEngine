#include <memory>
#include <gtest/gtest.h>
#include "RuntimeFixture.hpp"
#include "engine/components/ParticleComponent.hpp"

using ParticleComponentTest = RuntimeTest;

TEST_F(ParticleComponentTest, BurstRequestIsConsumedOnceAndNotCopied) {
    ParticleComponent emitter;
    emitter.EmitBurst(-1);
    emitter.EmitBurst(3);

    std::unique_ptr<ParticleComponent> copy(emitter.Copy());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->ConsumePendingBurst(), 0);
    EXPECT_EQ(emitter.ConsumePendingBurst(), 3);
    EXPECT_EQ(emitter.ConsumePendingBurst(), 0);
}

TEST_F(ParticleComponentTest, BurstRequestsAccumulateAndDoNotSerialize) {
    ParticleComponent emitter;
    emitter.EmitBurst(2);
    emitter.EmitBurst(4);
    emitter.EmitBurst(0);
    ParticleComponent restored;
    restored.Deserialize(emitter.Serialize());
    EXPECT_EQ(restored.ConsumePendingBurst(), 0);
    EXPECT_EQ(emitter.ConsumePendingBurst(), 6);
    EXPECT_EQ(emitter.ConsumePendingBurst(), 0);
}

TEST_F(ParticleComponentTest, CapacityIsNormalized) {
    ParticleComponent emitter;
    emitter.SetMaxParticles(-5);
    EXPECT_EQ(emitter.GetMaxParticles(), 1);
    emitter.SetMaxParticles(2000000);
    EXPECT_EQ(emitter.GetMaxParticles(), 1000000);
}

TEST_F(ParticleComponentTest, LegacyCudaEmitterLoadsAndResavesWithoutBackend) {
    ParticleComponent emitter;
    emitter.SetEmissionRate(12.0f);
    emitter.SetLifetime({0.5f, 3.0f});
    emitter.SetShape(ParticleComponent::Shape::Cone);
    emitter.SetSpace(ParticleComponent::SimulationSpace::Local);
    YAML::Node legacy = emitter.Serialize();
    legacy["simulationBackend"] = 1; // CUDA in older scenes.

    ParticleComponent restored;
    restored.Deserialize(YAML::Load(YAML::Dump(legacy)));

    const YAML::Node saved = YAML::Load(YAML::Dump(restored.Serialize()));
    EXPECT_FALSE(saved["simulationBackend"].IsDefined());
    ParticleComponent reopened;
    reopened.Deserialize(saved);

    for (const auto* result : {&restored, &reopened}) {
        EXPECT_FLOAT_EQ(result->GetEmissionRate(), 12.0f);
        EXPECT_EQ(result->GetLifetime(), glm::vec2(0.5f, 3.0f));
        EXPECT_EQ(result->GetShape(), ParticleComponent::Shape::Cone);
        EXPECT_EQ(result->GetSpace(), ParticleComponent::SimulationSpace::Local);
    }
}

TEST_F(ParticleComponentTest, CopyAndSerializationKeepAuthoredEmitterSettings) {
    ParticleComponent emitter;
    emitter.SetEmissionRate(12.0f);
    emitter.SetLifetime({0.5f, 3.0f});
    emitter.SetShape(ParticleComponent::Shape::Cone);
    emitter.SetFlipX(true);
    emitter.SetSortingLayer("Effects");
    std::unique_ptr<ParticleComponent> copy(emitter.Copy());
    ParticleComponent restored;
    restored.Deserialize(emitter.Serialize());
    for (const ParticleComponent* result : {copy.get(), &restored}) {
        ASSERT_NE(result, nullptr);
        EXPECT_FLOAT_EQ(result->GetEmissionRate(), 12.0f);
        EXPECT_EQ(result->GetLifetime(), glm::vec2(0.5f, 3.0f));
        EXPECT_EQ(result->GetShape(), ParticleComponent::Shape::Cone);
        EXPECT_TRUE(result->GetFlipX());
        EXPECT_EQ(result->GetSortingLayer(), "Effects");
    }
}

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

TEST_F(ParticleComponentTest, CapacityAndBackendAreNormalized) {
    ParticleComponent emitter;
    emitter.SetMaxParticles(-5);
    EXPECT_EQ(emitter.GetMaxParticles(), 1);
    emitter.SetMaxParticles(2000000);
    EXPECT_EQ(emitter.GetMaxParticles(), 1000000);
    emitter.SetSimulationBackend(ParticleComponent::SimulationBackend::CUDA);
    EXPECT_EQ(emitter.GetSimulationBackend(), ParticleComponent::SimulationBackend::CUDA);
    emitter.SetSimulationBackend(static_cast<ParticleComponent::SimulationBackend>(99));
    EXPECT_EQ(emitter.GetSimulationBackend(), ParticleComponent::SimulationBackend::OpenGLCompute);
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

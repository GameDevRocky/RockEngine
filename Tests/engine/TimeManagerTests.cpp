#include <doctest.h>

#include <chrono>
#include <thread>

#include "engine/core/Container.hpp"
#include "engine/core/TimeManager.hpp"

namespace {

void WaitForClockTick() {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

} // namespace

TEST_CASE("paused containers freeze scaled time but keep unscaled time running") {
    Container container(Container::Mode::Runtime);
    auto* time = new TimeManager();
    container.AddSystem(time);
    container.Init();

    WaitForClockTick();
    container.Update();
    REQUIRE(time->DeltaTime() > 0.0f);
    REQUIRE(time->ElapsedTime() > 0.0f);

    const float elapsedBeforePause = time->ElapsedTime();
    const float unscaledBeforePause = time->UnscaledTime();
    container.SetPaused(true);

    WaitForClockTick();
    container.Update();

    CHECK(time->DeltaTime() == 0.0f);
    CHECK(time->ElapsedTime() == elapsedBeforePause);
    CHECK(time->UnscaledDeltaTime() > 0.0f);
    CHECK(time->UnscaledTime() > unscaledBeforePause);

    container.Shutdown();
}

TEST_CASE("stepping a paused container advances scaled time by one fixed step") {
    Container container(Container::Mode::Runtime);
    auto* time = new TimeManager();
    container.AddSystem(time);
    container.Init();
    container.SetPaused(true);

    const float elapsedBeforeStep = time->ElapsedTime();
    const float unscaledBeforeStep = time->UnscaledTime();
    container.StepFrame();

    CHECK(time->DeltaTime() == time->FixedDeltaTime());
    CHECK(time->ElapsedTime() == doctest::Approx(elapsedBeforeStep + time->FixedDeltaTime()));
    CHECK(time->UnscaledTime() == unscaledBeforeStep);

    container.Shutdown();
}

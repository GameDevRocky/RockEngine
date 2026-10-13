#include <doctest.h>

#include "engine/core/InputManager.hpp"
#include "engine/input/CursorManager.hpp"

TEST_CASE("cursor state changes notify once and reset to safe defaults") {
    CursorManager& cursor = CursorManager::Get();
    cursor.Reset();

    int changes = 0;
    const int subscription = cursor.Subscribe([&]() {
        ++changes;
        return true;
    }, CursorManager::CHANGED_EVENT);

    cursor.SetVisible(false);
    cursor.SetVisible(false);
    cursor.SetLockState(CursorLockState::Locked);

    CHECK(changes == 2);
    CHECK_FALSE(cursor.IsVisible());
    CHECK(cursor.GetLockState() == CursorLockState::Locked);

    cursor.Reset();
    CHECK(changes == 3);
    CHECK(cursor.IsVisible());
    CHECK(cursor.GetLockState() == CursorLockState::None);
    CHECK(cursor.GetSpriteID().empty());

    cursor.Unsubscribe(subscription);
}

TEST_CASE("mouse delta accumulates native events for exactly one engine frame") {
    InputManager input;

    input.AddMouseDelta({2.0f, -3.0f});
    input.AddMouseDelta({4.5f, 1.0f});
    input.Update();

    CHECK(input.GetMouseDelta().x == doctest::Approx(6.5f));
    CHECK(input.GetMouseDelta().y == doctest::Approx(-2.0f));

    input.Update();
    CHECK(input.GetMouseDelta().x == doctest::Approx(0.0f));
    CHECK(input.GetMouseDelta().y == doctest::Approx(0.0f));
}

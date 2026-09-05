#include "engine/rendering/core/GridSettings.hpp"

#include <algorithm>

namespace {
// Clamped rather than merely guarded against zero: a snap step small enough to
// be meaningless still costs a rounding pass on every drag frame, and a grid
// cell below a pixel renders as a solid fill. The upper bounds keep a typo in a
// drag field from putting the grid somewhere the camera can never reach.
constexpr float kMinCell   = 1.0f,   kMaxCell   = 4096.0f;
constexpr float kMinMove   = 0.01f,  kMaxMove   = 4096.0f;
constexpr float kMinRotate = 1.0f,   kMaxRotate = 180.0f;
constexpr float kMinScale  = 0.01f,  kMaxScale  = 10.0f;

// Store, then notify only if the stored value actually moved.
//
// Clamping first is what makes this correct: typing past a bound leaves the
// field unchanged, and firing CHANGED_EVENT there would bounce the widget's
// text back to the same number on every keystroke. Comparing the CLAMPED value
// against the current one is also why a setter called with an out-of-range
// value twice stays silent the second time.
template <typename T>
bool Assign(T& field, T value) {
    if (field == value) return false;
    field = value;
    return true;
}
}

GridSettings& GridSettings::Get() {
    static GridSettings instance;
    return instance;
}

void GridSettings::SetVisible(bool v) {
    if (Assign(m_visible, v)) Notify(CHANGED_EVENT);
}

void GridSettings::SetSnapEnabled(bool v) {
    if (Assign(m_snapEnabled, v)) Notify(CHANGED_EVENT);
}

void GridSettings::SetCellSize(float v) {
    if (Assign(m_cellSize, std::clamp(v, kMinCell, kMaxCell))) Notify(CHANGED_EVENT);
}

void GridSettings::SetMoveSnap(float v) {
    if (Assign(m_moveSnap, std::clamp(v, kMinMove, kMaxMove))) Notify(CHANGED_EVENT);
}

void GridSettings::SetRotateSnap(float v) {
    if (Assign(m_rotateSnap, std::clamp(v, kMinRotate, kMaxRotate))) Notify(CHANGED_EVENT);
}

void GridSettings::SetScaleSnap(float v) {
    if (Assign(m_scaleSnap, std::clamp(v, kMinScale, kMaxScale))) Notify(CHANGED_EVENT);
}

void GridSettings::MatchMoveSnapToCell() {
    // Routed through the setter so the clamp and the notify are not duplicated
    // here -- cell size and move snap share the 0.01..4096 upper bound but not
    // the lower one, so assigning m_moveSnap directly could smuggle a value
    // below kMinMove past the clamp.
    SetMoveSnap(m_cellSize);
}

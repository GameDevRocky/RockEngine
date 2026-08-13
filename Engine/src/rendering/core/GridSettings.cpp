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
}

GridSettings& GridSettings::Get() {
    static GridSettings instance;
    return instance;
}

void GridSettings::SetCellSize(float v)   { m_cellSize   = std::clamp(v, kMinCell,   kMaxCell); }
void GridSettings::SetMoveSnap(float v)   { m_moveSnap   = std::clamp(v, kMinMove,   kMaxMove); }
void GridSettings::SetRotateSnap(float v) { m_rotateSnap = std::clamp(v, kMinRotate, kMaxRotate); }
void GridSettings::SetScaleSnap(float v)  { m_scaleSnap  = std::clamp(v, kMinScale,  kMaxScale); }

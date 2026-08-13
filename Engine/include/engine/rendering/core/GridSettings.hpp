#pragma once

// Editor grid + snapping settings, in one place.
//
// These values had been compile-time constants in two unrelated files:
// EngineUtils::RenderUtils::GridCellPixels drove both the grid shader's spacing
// and the gizmo's translate snap, while the rotate and scale increments were
// anonymous-namespace constants inside GizmosManager.cpp. Nothing could change
// them at runtime, and nothing tied the grid you see to the grid you snap to.
//
// A singleton outside any Container, alongside Renderer and AssetManager: like
// the render pipeline, this has no per-world identity, and it must survive the
// play-mode container swap. Read at draw time rather than pushed, the same
// pull-don't-push rule the cameras follow.
class GridSettings {
public:
    static GridSettings& Get();

    // ── Grid ────────────────────────────────────────────────────────────────
    bool  IsVisible() const   { return m_visible; }
    void  SetVisible(bool v)  { m_visible = v; }

    // World units per grid cell. Also the default translate snap, because a snap
    // that doesn't land on a visible line is worse than no snap at all.
    float GetCellSize() const { return m_cellSize; }
    void  SetCellSize(float v);

    // ── Snapping ────────────────────────────────────────────────────────────
    // The toolbar's persistent toggle. Holding Ctrl still snaps regardless --
    // the editor ORs the two, so the toggle means "always" and Ctrl means "just
    // now", and neither can lock the other out mid-drag.
    bool  IsSnapEnabled() const  { return m_snapEnabled; }
    void  SetSnapEnabled(bool v) { m_snapEnabled = v; }

    float GetMoveSnap() const   { return m_moveSnap; }
    void  SetMoveSnap(float v);

    float GetRotateSnap() const { return m_rotateSnap; }   // degrees
    void  SetRotateSnap(float v);

    float GetScaleSnap() const  { return m_scaleSnap; }
    void  SetScaleSnap(float v);

    // Re-link the translate snap to the cell size after either has been edited
    // apart. Exposed so the toolbar can offer it as an explicit action rather
    // than silently overwriting a value the user set on purpose.
    void  MatchMoveSnapToCell() { m_moveSnap = m_cellSize; }

private:
    GridSettings() = default;

    bool  m_visible     = true;
    float m_cellSize    = 32.0f;   // was EngineUtils::RenderUtils::GridCellPixels
    bool  m_snapEnabled = false;
    float m_moveSnap    = 32.0f;
    float m_rotateSnap  = 15.0f;   // was GizmosManager.cpp's kRotateSnapDeg
    float m_scaleSnap   = 0.25f;   // was GizmosManager.cpp's kScaleSnapStep
};

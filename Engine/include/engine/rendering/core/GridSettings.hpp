#pragma once
#include "engine/core/Observable.hpp"

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
//
// It is an Observable so UI can subscribe rather than re-read it every frame.
// The render side still PULLS at draw time (GridPass/GizmosManager) -- a pass
// runs once per view per frame, so a pull is always current and needs no
// notification. The event exists for the editor's widgets, which are otherwise
// only correct if they poll at the display refresh rate to catch a toggle a
// user flips a few times a minute.
class GridSettings : public Observable {
public:
    // Payload-free: subscribers re-read whichever values they care about. One
    // event for the whole struct rather than one per field -- these fire at
    // human speed, and a single coarse event cannot leave a listener half-synced.
    static inline const Event CHANGED_EVENT = Observable::CreateEvent();

    static GridSettings& Get();

    // ── Grid ────────────────────────────────────────────────────────────────
    bool  IsVisible() const   { return m_visible; }
    void  SetVisible(bool v);

    // World units per grid cell. Also the default translate snap, because a snap
    // that doesn't land on a visible line is worse than no snap at all.
    float GetCellSize() const { return m_cellSize; }
    void  SetCellSize(float v);

    // ── Snapping ────────────────────────────────────────────────────────────
    // The toolbar's persistent toggle. Holding Ctrl still snaps regardless --
    // the editor ORs the two, so the toggle means "always" and Ctrl means "just
    // now", and neither can lock the other out mid-drag.
    bool  IsSnapEnabled() const  { return m_snapEnabled; }
    void  SetSnapEnabled(bool v);

    float GetMoveSnap() const   { return m_moveSnap; }
    void  SetMoveSnap(float v);

    float GetRotateSnap() const { return m_rotateSnap; }   // degrees
    void  SetRotateSnap(float v);

    float GetScaleSnap() const  { return m_scaleSnap; }
    void  SetScaleSnap(float v);

    // Re-link the translate snap to the cell size after either has been edited
    // apart. Exposed so the toolbar can offer it as an explicit action rather
    // than silently overwriting a value the user set on purpose.
    void  MatchMoveSnapToCell();

private:
    GridSettings() = default;

    bool  m_visible     = true;
    float m_cellSize    = 32.0f;   // was EngineUtils::RenderUtils::GridCellPixels
    bool  m_snapEnabled = false;
    float m_moveSnap    = 32.0f;
    float m_rotateSnap  = 15.0f;   // was GizmosManager.cpp's kRotateSnapDeg
    float m_scaleSnap   = 0.25f;   // was GizmosManager.cpp's kScaleSnapStep
};

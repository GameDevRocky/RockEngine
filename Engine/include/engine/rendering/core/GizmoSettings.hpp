#pragma once
#include "engine/core/Observable.hpp"

// Which Scene-view gizmo overlays are drawn.
//
// GizmosManager used to draw every camera rect, light shape, audio ring, joint
// anchor, selection outline and component icon in the scene unconditionally,
// with no way to quiet any of it. A scene with a dozen lights and cameras
// becomes unreadable, and the overlays sit on top of the very sprites you are
// trying to place.
//
// A singleton outside any Container, alongside GridSettings/Renderer: like the
// render pipeline this has no per-world identity, and it must survive the
// play-mode container swap. GizmosManager PULLS it at draw time (a draw runs
// once per frame, so a pull is always current); the CHANGED_EVENT exists for
// the editor's toolbar, which would otherwise have to poll.
//
// The transform / collider / sprite-box manipulators are deliberately NOT
// represented here. Those are tools, not overlays -- hiding the handle you are
// dragging is never what "hide gizmos" is meant to mean.
class GizmoSettings : public Observable {
public:
    // Everything DrawGizmos draws that is not a manipulator. Cameras, Lights and
    // AudioSources own drag state; the rest are read-only (see GizmosManager).
    enum class Category {
        Cameras,            // camera view-region rects
        Lights,             // Light shapes + ShadowCaster outlines
        AudioSources,       // min/max attenuation rings
        Joints,             // joint anchors + link line
        ComponentIcons,     // billboarded component-type icons
        SelectionOutlines,  // white outline around each selected sprite
        Count
    };

    // Payload-free: subscribers re-read whatever they care about. One event for
    // the whole struct rather than one per category -- these fire at human
    // speed, and a single coarse event cannot leave a listener half-synced.
    static inline const Event CHANGED_EVENT = Observable::CreateEvent();

    static GizmoSettings& Get();

    // Master toggle. Off hides every category at once without disturbing which
    // individual ones are ticked, so turning it back on restores the set the
    // user had chosen rather than resetting to all-on.
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool v);

    bool IsCategoryVisible(Category c) const;
    void SetCategoryVisible(Category c, bool v);

    // The only thing GizmosManager should call: master AND category.
    bool ShouldDraw(Category c) const { return m_enabled && IsCategoryVisible(c); }

    // Menu label for a category. Lives here so adding a Category needs no edit
    // on the editor side -- the toolbar builds its menu by looping to Count.
    static const char* CategoryLabel(Category c);

private:
    GizmoSettings() = default;

    static constexpr int kCount = static_cast<int>(Category::Count);

    // All-on by default: unchanged behaviour until something is unticked.
    bool m_enabled = true;
    bool m_categories[kCount] = { true, true, true, true, true, true };
};

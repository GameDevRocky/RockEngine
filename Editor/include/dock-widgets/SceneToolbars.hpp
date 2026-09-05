#pragma once
#include <QWidget>
#include <QFont>
#include <QPoint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "Engine.hpp"                        // EngineUtils::Proxy
#include "engine/core/Observable.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/utils/Properties.hpp"
#include "utils/ProperyFactory.hpp"          // PropertyWidget<float>

class QBoxLayout;
class QToolButton;
class QMouseEvent;

// ─────────────────────────────────────────────────────────────────────────────
// Floating toolbars overlaid on the Scene view.
//
// These are plain QWidgets absolutely positioned over the GL viewport, not
// QToolBars and not docks -- they are children of SceneViewGui and are dragged
// around inside it. Nothing about that is automatic: the host has to construct
// them, and re-clamp them on resize.
//
// No Q_OBJECT anywhere in this file. Nothing here declares a signal; every
// connect() is to a lambda, for which the sender's own metaobject suffices.
// ─────────────────────────────────────────────────────────────────────────────

// Shared chrome: the drag handle, collapse arrow, styling, icon font, and the
// control-construction helpers. Orientation-aware so the same class backs both
// the vertical tool palette and the horizontal view-options bar.
class SceneOverlayToolbar : public QWidget {
public:
    ~SceneOverlayToolbar() override;

    void SetCollapsed(bool collapsed);
    void ClampToParent();

    // Push current engine state into the widgets.
    //
    // This is NOT a per-frame poll -- it runs from the constructor and from the
    // event handlers registered with SubscribeTo(). The toolbar it replaced
    // re-read every setting at the display's refresh rate because GridSettings
    // had no notifications; it does now, as do GizmoSettings and GizmosManager.
    virtual void Refresh();

protected:
    SceneOverlayToolbar(QWidget* parent, Qt::Orientation orientation);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    // ── Control construction (call from a subclass constructor) ──────────────
    QToolButton* AddButton(const char* icon, const char* tooltip);
    void AddSeparator();
    void BindFloatProperty(std::function<float()> getter,
                           std::function<void(float)> setter,
                           const Properties::PropDesc& desc,
                           const char* tooltip);

    // Subscribe `handler` to `event` on `source` and remember the handle so the
    // destructor can unsubscribe. The wrapper always returns true: an Observable
    // callback returning false auto-unsubscribes, which here would quietly leave
    // the toolbar frozen on stale state.
    //
    // Only for sources that outlive this widget -- the process-global settings
    // singletons and Engine. Anything Container-scoped (and therefore deleted by
    // the play-mode swap) needs the hand-managed treatment SceneToolsToolbar
    // gives its per-object subscriptions.
    void SubscribeTo(Observable* source, Observable::Event event, std::function<void()> handler);

    bool IsHorizontal() const { return m_orientation == Qt::Horizontal; }

    const QFont& IconFont() const { return m_iconFont; }

    static constexpr int kControlHeight = 24;
    static constexpr int kIconWidth     = 28;
    // Numeric fields get more room when laid out horizontally: 28px cannot show
    // a 4-digit cell size, and the horizontal bar has the space to spare.
    static constexpr int kFieldWidthHorizontal = 52;

private:
    struct FloatBinding {
        std::unique_ptr<PropertyWidget<float>> property;
        std::function<float()> getter;
        QWidget* widget = nullptr;
    };

    Qt::Orientation m_orientation;
    QFont        m_iconFont;
    QWidget*     m_contents       = nullptr;
    QBoxLayout*  m_controlsLayout = nullptr;
    QToolButton* m_collapse       = nullptr;

    std::vector<FloatBinding> m_floatBindings;
    std::vector<std::pair<Observable*, int>> m_subs;

    QPoint m_dragOffset;
    bool   m_dragging  = false;
    bool   m_collapsed = false;
};

// ── Vertical: the transform tool palette ─────────────────────────────────────
// Grid and snapping used to live here too; they moved to SceneViewOptionsToolbar
// so this bar is only ever about "which tool is active".
class SceneToolsToolbar final : public SceneOverlayToolbar {
public:
    explicit SceneToolsToolbar(QWidget* parent);
    ~SceneToolsToolbar() override;

    void Refresh() override;

private:
    // The sprite-box and collider buttons appear only when the selected object
    // actually has the component they edit, so this bar has to follow both the
    // selection AND that object's component list.
    void SubscribeToSelector();
    void OnSelectionChanged();
    // Re-point the ADD/REMOVE_COMPONENT subscriptions at the currently selected
    // object. `unsubscribeOld` is false only on the play-mode exit path, where
    // the previously-subscribed objects have already been freed.
    void RewireSelectionSubs(bool unsubscribeOld);

    QToolButton* m_hand      = nullptr;
    QToolButton* m_universal = nullptr;
    QToolButton* m_translate = nullptr;
    QToolButton* m_rotate    = nullptr;
    QToolButton* m_scale     = nullptr;
    QToolButton* m_spriteBox = nullptr;
    QToolButton* m_collider  = nullptr;

    EngineUtils::Proxy<SelectionManager> m_selection;
    // Subscriptions on the SELECTED GAMEOBJECT, not on long-lived singletons --
    // hand-managed rather than going through SubscribeTo() because the object
    // they point at changes with the selection and dies with the play-mode
    // container. See RewireSelectionSubs.
    std::vector<std::pair<Observable*, int>> m_objectSubs;
};

// ── Horizontal: what the Scene view shows ────────────────────────────────────
// Lighting (shaded/unshaded), gizmo overlay visibility, and the grid + snapping
// block moved out of the tool palette.
class SceneViewOptionsToolbar final : public SceneOverlayToolbar {
public:
    explicit SceneViewOptionsToolbar(QWidget* parent);

    void Refresh() override;

private:
    void ShowGizmoMenu();

    QToolButton* m_lighting  = nullptr;
    QToolButton* m_gizmos    = nullptr;
    QToolButton* m_gizmoMenu = nullptr;
    QToolButton* m_snap      = nullptr;
    QToolButton* m_grid      = nullptr;
    QToolButton* m_match     = nullptr;
};

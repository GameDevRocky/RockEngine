#pragma once
#include <QWidget>
#include <QPointer>
#include <string>
#include <vector>
#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"

class QStackedWidget;
class QLabel;
class QGridLayout;
class Animator;
class AnimatorGraphCanvas;
class FrameListWidget;

using namespace EngineUtils;

// The Animator editor panel -- a singleton dockable widget, like SceneViewGui,
// opened from the Animator inspector's "Open Animator" button. It follows the
// selection: when the primary selected GameObject has an Animator it shows the
// node-graph state-machine editor for it, otherwise "No Animator Selected". The
// edit page is a 3-pane split: parameters | node canvas | selection inspector.
class AnimatorGui : public QWidget {
    Q_OBJECT

public:
    static AnimatorGui* Get() {
        static AnimatorGui* instance = nullptr;
        if (!instance) instance = new AnimatorGui(nullptr);
        return instance;
    }
    void Init();

    // Force the panel to re-read the current selection now (the "Open Animator"
    // button calls this; adding a component fires no selection event).
    void SyncToSelection();

private:
    explicit AnimatorGui(QWidget* parent = nullptr);
    ~AnimatorGui() override = default;

    void BuildEditPage();
    void SubscribeToSelector();
    void UpdateForSelection();
    void ClearAnimatorSubscriptions();
    bool IsPlayMode() const;

    // Side panels (rebuilt as the graph/selection change).
    void RebuildParametersPanel();
    void ClearInspector();
    void ShowStateInspector(const std::string& stateName);
    void ShowTransitionInspector(const std::string& transitionId);
    // Queued rebuild of just the frame strip after a structural frame edit.
    void RefreshFrameList();

    // Row helpers mirroring the object Inspector's grid: bold label left, editor
    // right, with full-width rows reserved for things that need the whole panel
    // (section titles, the frame strip). Keeping the two panels on these instead
    // of ad-hoc layouts is what makes them read as the same UI.
    void AddRow(QGridLayout* grid, int& row, const std::string& label, QWidget* widget);
    void AddFullRow(QGridLayout* grid, int& row, QWidget* widget);
    void AddSectionTitle(QGridLayout* grid, int& row, const QString& text, bool spaceAbove = true);
    // Absorbs leftover vertical space so rows stay compact and top-aligned instead
    // of spreading out over the panel.
    void AddTrailingStretch(QGridLayout* grid, int& row);

    QStackedWidget* m_stack = nullptr;
    QWidget* m_emptyPage = nullptr;    // "No Animator Selected"
    QWidget* m_editPage = nullptr;
    QLabel*  m_editHeader = nullptr;

    AnimatorGraphCanvas* m_canvas = nullptr;
    QGridLayout* m_paramsLayout = nullptr;      // parameter rows go here
    QGridLayout* m_inspectorLayout = nullptr;   // per-selection widgets go here
    int m_paramsRow = 0;
    int m_inspectorRow = 0;
    // Rebuilt in place on frame add/remove/reorder, so editing frames doesn't tear
    // down (and steal focus from) the rest of the state inspector.
    QPointer<FrameListWidget> m_frameList;

    Animator* m_animator = nullptr;    // currently-edited animator (resolved each UpdateForSelection)
    std::string m_animatorId;          // its component id (re-resolve for safe unsubscribe)
    std::vector<int> m_animatorSubs;   // event subscriptions on the current animator

    // What the right-hand inspector is currently showing (for refresh after edits).
    std::string m_inspectorState;
    std::string m_inspectorTransitionId;

    Proxy<SelectionManager> selectionManager;
};

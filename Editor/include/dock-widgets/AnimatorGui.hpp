#pragma once
#include <QWidget>
#include <string>
#include "Engine.hpp"
#include "engine/core/SelectionManager.hpp"

class QStackedWidget;
class QLabel;
class Animator;

using namespace EngineUtils;

// The Animator editor panel -- a singleton dockable widget, like SceneViewGui,
// opened on demand from the Animator component's inspector "Open Animator" button
// (see MainWindow::ShowAnimator). It follows the selection: when the primary
// selected GameObject has an Animator it shows the editor for it, otherwise
// "No Animator Selected". The actual graph/frame editor is a placeholder for now.
class AnimatorGui : public QWidget {
    Q_OBJECT

public:
    static AnimatorGui* Get() {
        static AnimatorGui* instance = nullptr;
        if (!instance) instance = new AnimatorGui(nullptr);
        return instance;
    }
    void Init();

    // Force the panel to re-read the current selection immediately. The "Open
    // Animator" button calls this because adding an Animator to the already-
    // selected object fires no SELECTION_CHANGED_EVENT, so without it the panel
    // would still show its stale "No Animator Selected" state when first opened.
    void SyncToSelection();

private:
    explicit AnimatorGui(QWidget* parent = nullptr);
    ~AnimatorGui() override = default;

    // (Re)subscribe to the active container's SelectionManager -- re-run on
    // play-mode enter (fresh runtime manager), mirroring InspectorGui.
    void SubscribeToSelector();
    // Resolve the primary selection and switch pages; (re)wires the shown
    // Animator's SHUTDOWN_EVENT watch.
    void UpdateForSelection();
    // Drop the SHUTDOWN_EVENT subscription on the currently-shown Animator.
    void ClearAnimatorSubscription();

    QStackedWidget* m_stack = nullptr;
    QWidget* m_emptyPage = nullptr;   // "No Animator Selected"
    QWidget* m_editPage = nullptr;    // placeholder editor
    QLabel*  m_editHeader = nullptr;  // names the animator being edited

    std::string m_animatorId;         // id of the Animator currently shown ("" = none)
    int m_animatorShutdownSub = -1;   // its SHUTDOWN_EVENT subscription

    Proxy<SelectionManager> selectionManager;
};

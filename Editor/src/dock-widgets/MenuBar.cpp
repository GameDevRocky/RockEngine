#include "dock-widgets/MenuBar.hpp"
#include "dock-widgets/BuildWindow.hpp"
#include <QKeySequence>
#include "Engine.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/UndoSystem.hpp"
#include "utils/NavigationHistory.hpp"

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent) {}

namespace {
// The undo history lives on the active container, so this follows the play-mode
// swap: during play it is the runtime container's (empty at first) history.
UndoSystem* ActiveUndoSystem() {
    Container* active = Engine::Get()->GetActiveContainer();
    return active ? active->FindSystem<UndoSystem>() : nullptr;
}
} // namespace

void MenuBar::Init() {
    if (initialized_) {
        return;
    }

    fileMenu = addMenu("File");
    editMenu = addMenu("Edit");
    windowMenu = addMenu("Window");
    helpMenu = addMenu("Help");

    newSceneAction = fileMenu->addAction("New Scene");
    openSceneAction = fileMenu->addAction("Open Scene");
    saveSceneAction = fileMenu->addAction("Save Scene");
    saveSceneAsAction = fileMenu->addAction("Save Scene As...");

    fileMenu->addSeparator();
    buildGameAction = fileMenu->addAction("Build Game...");
    // Application-scoped for the same reason Undo/Redo are (see below): a WindowShortcut
    // would not fire while a QOpenGLWidget viewport has focus, which is most of the time.
    buildGameAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    buildGameAction->setShortcutContext(Qt::ApplicationShortcut);
    // Connected with a direct lambda, deliberately NOT via a *Requested signal like the
    // scene actions above -- those signals have no receivers anywhere in the codebase, so
    // that whole half of this menu does nothing when clicked. Don't copy that pattern.
    connect(buildGameAction, &QAction::triggered, this, []() {
        BuildWindow::Get()->ShowCentered();
    });

    fileMenu->addSeparator();
    exitAction = fileMenu->addAction("Exit");

    undoAction = editMenu->addAction("Undo");
    redoAction = editMenu->addAction("Redo");

    undoAction->setShortcut(QKeySequence::Undo);                    // Ctrl+Z
    redoAction->setShortcuts({QKeySequence::Redo,                   // Ctrl+Y on Windows
                              QKeySequence(QStringLiteral("Ctrl+Shift+Z"))});

    // Application-scoped so the shortcut works whichever panel has focus; a
    // WindowShortcut would not fire while a QOpenGLWidget viewport is focused.
    //
    // Two consequences worth knowing before "fixing" this:
    //   - Qt consumes a matching shortcut before keyPressEvent runs, so Ctrl+Z
    //     never reaches SceneViewGui/GameViewGui -> InputManager. That is what we
    //     want, and because the press never registers there is no stuck key.
    //   - It also overrides QLineEdit's built-in per-field text undo in the
    //     inspector. Intentional, and what Unity and Godot do.
    undoAction->setShortcutContext(Qt::ApplicationShortcut);
    redoAction->setShortcutContext(Qt::ApplicationShortcut);

    connect(undoAction, &QAction::triggered, this, []() {
        if (auto* undoSystem = ActiveUndoSystem()) undoSystem->Undo();
    });
    connect(redoAction, &QAction::triggered, this, []() {
        if (auto* undoSystem = ActiveUndoSystem()) undoSystem->Redo();
    });

    editMenu->addSeparator();

    // Navigation, deliberately SEPARATE from Undo/Redo rather than folded into it.
    // Back/Forward move through view state -- which viewport tab is raised and what is
    // selected -- while Undo moves through changes to the document. Sharing one stack
    // would let a minute of clicking around push real edits off the end of the 100-entry
    // undo history, and would make Ctrl+Z unpredictable. See NavigationHistory.
    backAction = editMenu->addAction("Back");
    forwardAction = editMenu->addAction("Forward");

    // Alt+Left / Alt+Right: the browser convention, and free of collisions here.
    backAction->setShortcut(QKeySequence(QStringLiteral("Alt+Left")));
    forwardAction->setShortcut(QKeySequence(QStringLiteral("Alt+Right")));

    // Application-scoped for the same reason Undo/Redo are: a WindowShortcut would not
    // fire while a QOpenGLWidget viewport has focus, which is exactly where you are when
    // you want to step back to the last thing you had selected.
    backAction->setShortcutContext(Qt::ApplicationShortcut);
    forwardAction->setShortcutContext(Qt::ApplicationShortcut);

    connect(backAction, &QAction::triggered, this, []() {
        NavigationHistory::Get()->Back();
    });
    connect(forwardAction, &QAction::triggered, this, []() {
        NavigationHistory::Get()->Forward();
    });

    resetLayoutAction = windowMenu->addAction("Reset Layout");

    aboutAction = helpMenu->addAction("About RockEngine");

    connect(newSceneAction, &QAction::triggered, this, &MenuBar::NewSceneRequested);
    connect(openSceneAction, &QAction::triggered, this, &MenuBar::OpenSceneRequested);
    connect(saveSceneAction, &QAction::triggered, this, &MenuBar::SaveSceneRequested);
    connect(saveSceneAsAction, &QAction::triggered, this, &MenuBar::SaveSceneAsRequested);
    connect(exitAction, &QAction::triggered, this, &MenuBar::ExitRequested);
    connect(resetLayoutAction, &QAction::triggered, this, &MenuBar::ResetLayoutRequested);
    connect(aboutAction, &QAction::triggered, this, &MenuBar::AboutRequested);

    RefreshUndoActions();
    RefreshNavigationActions();
    initialized_ = true;
}

void MenuBar::PostInit() {
    // Deferred until the engine container exists. Each container owns its own
    // UndoSystem, so the subscription has to be remade on both sides of the
    // play-mode swap — the editor's system object is not the runtime one.
    SubscribeToUndoSystem();

    // Same deferral, same reason: the navigation history needs the container's
    // SelectionManager. It handles its own play-mode re-subscription internally.
    NavigationHistory::Get()->PostInit();
    connect(NavigationHistory::Get(), &NavigationHistory::HistoryChanged,
            this, &MenuBar::RefreshNavigationActions);
    RefreshNavigationActions();

    Engine::Get()->Subscribe([this]() {
        SubscribeToUndoSystem();
        RefreshUndoActions();
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);

    Engine::Get()->Subscribe([this]() {
        SubscribeToUndoSystem();
        RefreshUndoActions();
        return true;
    }, Engine::EXIT_PLAY_MODE_EVENT);
}

void MenuBar::AddWindowAction(QAction* action) {
    if (windowMenu && action && !windowMenu->actions().contains(action))
        windowMenu->addAction(action);
}

void MenuBar::SubscribeToUndoSystem() {
    // The previous container may already be gone (ExitPlayMode deletes the runtime
    // one), so don't try to unsubscribe from it — just drop the stale id. The dead
    // system took its subscriber list with it.
    m_undoSubId = -1;

    if (auto* undoSystem = ActiveUndoSystem()) {
        m_undoSubId = undoSystem->Subscribe([this]() {
            RefreshUndoActions();
            return true;
        }, UndoSystem::STACK_CHANGED_EVENT);
    }
}

void MenuBar::RefreshUndoActions() {
    if (!undoAction || !redoAction) return;

    auto* undoSystem = ActiveUndoSystem();
    const bool canUndo = undoSystem && undoSystem->CanUndo();
    const bool canRedo = undoSystem && undoSystem->CanRedo();

    undoAction->setEnabled(canUndo);
    redoAction->setEnabled(canRedo);

    // Name the specific edit, the way QUndoStack::createUndoAction used to.
    undoAction->setText(canUndo
        ? QStringLiteral("Undo %1").arg(QString::fromStdString(undoSystem->UndoText()))
        : QStringLiteral("Undo"));
    redoAction->setText(canRedo
        ? QStringLiteral("Redo %1").arg(QString::fromStdString(undoSystem->RedoText()))
        : QStringLiteral("Redo"));
}

void MenuBar::RefreshNavigationActions() {
    if (!backAction || !forwardAction) return;

    auto* history = NavigationHistory::Get();
    backAction->setEnabled(history->CanGoBack());
    forwardAction->setEnabled(history->CanGoForward());
}

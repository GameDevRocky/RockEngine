#pragma once

#include <QObject>
#include <QPointer>

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

class QDockWidget;

// Browser-style Back/Forward through editor VIEW state: which viewport tab is raised,
// and what is selected.
//
// Deliberately NOT the UndoSystem, and the distinction is the whole point of this class.
// Undo is for changes to the document; this is for "where was I looking". Three concrete
// reasons they must not share a stack:
//
//   * UndoSystem::kHistoryLimit is 100 and the cap drops the OLDEST entry. Tab switches
//     and selection clicks are constant and nearly free; real edits are rare and
//     expensive. Mixed together, a minute of clicking around silently pushes genuine
//     work off the end of the history.
//   * Ctrl+Z is only useful while it is predictable. Once it can raise a tab, you have
//     to look before you press it.
//   * UndoSystem::BreakMergeChain() is called on selection change precisely because the
//     system treats selection as the BOUNDARY between edits. Making it an edit itself
//     inverts that.
//
// Editor-only and session-only by design. It is not a Container System and never enters
// the engine: view state has no per-world identity, is not serialized, and must not be
// deep-copied into play mode -- the same reasoning that keeps EditorCamera outside the
// ECS. It lives here with the Qt widgets it drives.
class NavigationHistory : public QObject {
    Q_OBJECT

public:
    static NavigationHistory* Get();

    // Same ceiling as the undo stack, for the same reason: entries are cheap but not
    // free, and an editing session is unbounded.
    static constexpr std::size_t kHistoryLimit = 100;

    // Docks whose raised/tabbed state is worth remembering (Scene, Game, Animator).
    // Held as QPointer so a dock destroyed later reads as null rather than dangling.
    void RegisterViewportDock(QDockWidget* dock);

    // Starts recording and seeds the first entry. Deferred because it needs both the
    // engine container (for SelectionManager) and the finished dock layout -- the
    // raise() calls during MainWindowGui construction must not become history.
    void PostInit();

    void Back();
    void Forward();

    bool CanGoBack() const { return m_cursor > 0; }
    bool CanGoForward() const { return !m_entries.empty() && m_cursor + 1 < m_entries.size(); }

    // Drops everything and re-seeds from the current state.
    void Reset();

    // Capture the current view state as a new entry, if it differs from the one the
    // cursor is on. Safe to call from any change signal -- it self-filters.
    void RecordCurrent();

signals:
    // Any change to the history or the cursor. MenuBar uses this to enable/disable its
    // Back/Forward actions.
    void HistoryChanged();

private:
    struct State {
        // The raised viewport dock's objectName, not a pointer or a tab index: names
        // survive docks being moved, floated, re-tabbed or rebuilt by a layout reset.
        std::string viewportDock;
        // Ordered, matching SelectionManager -- the last entry is the primary, so
        // restoring the order restores which object the Inspector shows.
        std::vector<std::string> selection;

        bool operator==(const State& other) const {
            return viewportDock == other.viewportDock && selection == other.selection;
        }
    };

    explicit NavigationHistory(QObject* parent = nullptr);
    ~NavigationHistory() override = default;

    State Capture() const;
    void Apply(const State& state);

    // (Re)subscribes to the active container's SelectionManager. Must run again after
    // each play-mode swap -- the two containers own separate managers, exactly as
    // MenuBar has to re-subscribe to their separate UndoSystems.
    void SubscribeToSelection();

    std::vector<QPointer<QDockWidget>> m_viewportDocks;

    std::deque<State> m_entries;
    std::size_t m_cursor = 0;

    // True while Back()/Forward() is applying a state. Everything Apply() does --
    // raising a dock, setting the selection -- fires the very signals that feed
    // RecordCurrent(), so without this a single Back() would record itself as a new
    // destination and Forward would never have anywhere to go.
    bool m_applying = false;

    // Recording is off until PostInit so construction-time layout does not seed junk.
    bool m_active = false;

    int m_selectionSubId = -1;
};

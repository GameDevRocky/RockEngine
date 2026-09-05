#include "utils/NavigationHistory.hpp"

#include <QDockWidget>

#include <algorithm>

#include "Engine.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/SelectionManager.hpp"

namespace {

SelectionManager* ActiveSelection() {
    Container* active = Engine::Get()->GetActiveContainer();
    return active ? active->FindSystem<SelectionManager>() : nullptr;
}

} // namespace

NavigationHistory* NavigationHistory::Get() {
    static NavigationHistory* instance = nullptr;
    if (!instance)
        instance = new NavigationHistory(nullptr);
    return instance;
}

NavigationHistory::NavigationHistory(QObject* parent)
    : QObject(parent)
{}

void NavigationHistory::RegisterViewportDock(QDockWidget* dock) {
    if (!dock)
        return;
    for (const auto& existing : m_viewportDocks) {
        if (existing == dock)
            return;
    }
    m_viewportDocks.emplace_back(dock);
}

void NavigationHistory::PostInit() {
    SubscribeToSelection();

    // Each container owns its own SelectionManager, so the subscription has to be remade
    // on both sides of the play-mode swap -- the editor's manager is not the runtime one.
    // The history is dropped at the same time: ids survive the deep copy, so old entries
    // would still resolve, and silently navigating you back into a selection captured in
    // the other world is worse than simply starting fresh. Same call the UndoSystem makes
    // by handing the runtime container an empty stack.
    Engine::Get()->Subscribe([this]() {
        SubscribeToSelection();
        Reset();
        return true;
    }, Engine::ENTER_PLAY_MODE_EVENT);

    Engine::Get()->Subscribe([this]() {
        SubscribeToSelection();
        Reset();
        return true;
    }, Engine::EXIT_PLAY_MODE_EVENT);

    m_active = true;
    Reset();
}

void NavigationHistory::SubscribeToSelection() {
    // The previous container may already be gone (ExitPlayMode deletes the runtime one),
    // so don't try to unsubscribe from it -- just drop the stale id. The dead system took
    // its subscriber list with it. Same handling as MenuBar::SubscribeToUndoSystem.
    m_selectionSubId = -1;

    if (auto* selection = ActiveSelection()) {
        m_selectionSubId = selection->Subscribe([this](std::any) {
            // Read-only: SELECTION_CHANGED_EVENT handlers must not mutate the selection,
            // because Notify dispatches over a copy of the callback list and a re-entrant
            // change would leave later handlers reading state that no longer matches the
            // payload they were handed. Recording only captures.
            RecordCurrent();
            return true;
        }, SelectionManager::SELECTION_CHANGED_EVENT);
    }
}

NavigationHistory::State NavigationHistory::Capture() const {
    State state;

    // Among tabified docks only the raised one is visible, so visibility IS "which tab is
    // in front" -- the same fact MainWindowGui's visibilityChanged handlers rely on to
    // pick the frame driver.
    for (const auto& dock : m_viewportDocks) {
        if (dock && dock->isVisible()) {
            state.viewportDock = dock->objectName().toStdString();
            break;
        }
    }

    if (auto* selection = ActiveSelection())
        state.selection = selection->GetSelectedIds();

    return state;
}

void NavigationHistory::RecordCurrent() {
    if (!m_active || m_applying)
        return;

    State state = Capture();

    // A no-op move is not a destination. Without this, clicking the already-selected
    // object or raising the already-raised tab would fill the history with duplicates
    // and Back would appear to do nothing for several presses.
    if (!m_entries.empty() && m_entries[m_cursor] == state)
        return;

    // Standard browser semantics: navigating somewhere new from a point in the middle
    // discards the forward entries. Keeping them would offer a Forward that jumps to a
    // branch the user has already navigated away from.
    if (!m_entries.empty() && m_cursor + 1 < m_entries.size())
        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(m_cursor) + 1,
                        m_entries.end());

    m_entries.push_back(std::move(state));

    // Cap from the front, which shifts the cursor down with it.
    while (m_entries.size() > kHistoryLimit)
        m_entries.pop_front();

    m_cursor = m_entries.size() - 1;
    emit HistoryChanged();
}

void NavigationHistory::Back() {
    if (!CanGoBack())
        return;
    --m_cursor;
    Apply(m_entries[m_cursor]);
    emit HistoryChanged();
}

void NavigationHistory::Forward() {
    if (!CanGoForward())
        return;
    ++m_cursor;
    Apply(m_entries[m_cursor]);
    emit HistoryChanged();
}

void NavigationHistory::Apply(const State& state) {
    m_applying = true;

    if (!state.viewportDock.empty()) {
        const QString wanted = QString::fromStdString(state.viewportDock);
        for (const auto& dock : m_viewportDocks) {
            if (!dock || dock->objectName() != wanted)
                continue;
            // show() first: a dock the user closed entirely cannot be raised, and
            // raise() alone on a hidden dock silently does nothing.
            dock->show();
            dock->raise();
            break;
        }
    }

    if (auto* selection = ActiveSelection()) {
        // SelectMany drops ids that no longer resolve and no-ops when the result is
        // unchanged, so a destroyed object simply drops out of a restored selection
        // rather than needing to be filtered here or invalidating the whole entry.
        selection->SelectMany(state.selection);
    }

    m_applying = false;
}

void NavigationHistory::Reset() {
    m_entries.clear();
    m_cursor = 0;

    // Seed the current position so the very first navigation has an origin to return to.
    if (m_active)
        m_entries.push_back(Capture());

    emit HistoryChanged();
}

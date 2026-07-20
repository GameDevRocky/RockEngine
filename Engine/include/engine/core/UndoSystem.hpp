#pragma once
#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include "engine/core/System.hpp"
#include "engine/core/Command.hpp"

// Per-container undo/redo history.
//
// Being a System is what makes the play-mode story work without special cases.
// Copy() returns an EMPTY stack, so entering play mode gives the runtime container
// a fresh history: the editor's is untouched behind it, runtime edits accumulate
// only in the throwaway world, and both die with their container. Commands resolve
// through the container that owns them (Command::Undo takes it), so a stack can
// never reach into the other world even in principle.
//
// Deliberately not std::stack: the history has to be capped, and capping means
// dropping the OLDEST entry, which std::stack cannot express. std::deque gives
// push/pop at the back plus pop_front() for the cap.
class UndoSystem : public System {
public:
    // Fired after any change to either stack: push, undo, redo, clear. Payload: none.
    // The editor's menu subscribes to this to update its Undo/Redo entries.
    static inline const Event STACK_CHANGED_EVENT = Observable::CreateEvent();

    // Commands can hold whole YAML subtrees, so an uncapped history is an
    // unbounded leak across a long editing session.
    static constexpr std::size_t kHistoryLimit = 100;

    // How long a coalescing gesture stays open. A continuous spinbox drag or an
    // uninterrupted typing run folds into one entry; a pause splits it.
    static constexpr std::chrono::milliseconds kMergeWindow{400};

    UndoSystem() = default;
    ~UndoSystem() override = default;

    void Init() override;
    void Shutdown() override;

    UndoSystem* Copy() override;
    UndoSystem* Copy(Container* container) override;

    // Records an edit the caller has ALREADY performed. Push never executes the
    // command — that keeps "do" and "record" separate and means a command whose
    // action was already applied needs no special first-redo case.
    // No-ops (and drops the command) while an undo/redo is being applied.
    void Push(std::unique_ptr<Command> command);

    void Undo();
    void Redo();
    void Clear();

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }

    // Empty when the corresponding stack is empty.
    std::string UndoText() const;
    std::string RedoText() const;

    // Ends the open coalescing gesture so the next Push starts a new entry rather
    // than folding into the last one. Called on selection change (subscribed in
    // Init) and, from the editor, on inspector rebuild and focus change.
    void BreakMergeChain() { ++m_gestureSerial; }
    unsigned GetGestureSerial() const { return m_gestureSerial; }

    // True while Undo()/Redo() is running. Callers that observe engine change
    // events use this to avoid recording a replay as a fresh edit.
    bool IsApplying() const { return m_applying; }

private:
    using Clock = std::chrono::steady_clock;

    bool TryMergeIntoTop(Command* incoming) const;

    std::deque<std::unique_ptr<Command>> m_undo;
    std::deque<std::unique_ptr<Command>> m_redo;

    bool m_applying = false;
    unsigned m_gestureSerial = 0;
    Clock::time_point m_lastPush{};
    int m_selectionSubId = -1;
};

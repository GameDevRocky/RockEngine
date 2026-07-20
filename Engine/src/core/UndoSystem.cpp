#include "engine/core/UndoSystem.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/SelectionManager.hpp"

void UndoSystem::Init()
{
    m_undo.clear();
    m_redo.clear();
    m_gestureSerial = 0;
    m_applying = false;

    // Retargeting is a gesture boundary: edits to two different objects must never
    // coalesce, even when they land inside the merge window.
    if (auto* selection = container->FindSystem<SelectionManager>()) {
        m_selectionSubId = selection->Subscribe([this](const std::any&) {
            BreakMergeChain();
            return true;
        }, SelectionManager::SELECTION_CHANGED_EVENT);
    }
}

void UndoSystem::Shutdown()
{
    m_undo.clear();
    m_redo.clear();
    m_selectionSubId = -1;
}

UndoSystem* UndoSystem::Copy()
{
    // Deliberately empty rather than a deep copy. History belongs to the world it
    // was recorded against; the runtime container is a throwaway copy, so it starts
    // with a clean slate and its edits are discarded along with it.
    return new UndoSystem();
}

UndoSystem* UndoSystem::Copy(Container* container)
{
    auto* copy = new UndoSystem();
    copy->Attach(container);
    return copy;
}

bool UndoSystem::TryMergeIntoTop(Command* incoming) const
{
    if (m_undo.empty()) return false;
    if (incoming->MergeId() == Command::kNoMerge) return false;

    Command* top = m_undo.back().get();
    if (top->MergeId() != incoming->MergeId()) return false;
    if (top->GetGestureSerial() != m_gestureSerial) return false;
    if (Clock::now() - m_lastPush > kMergeWindow) return false;

    return top->MergeWith(incoming);
}

void UndoSystem::Push(std::unique_ptr<Command> command)
{
    if (!command) return;

    // A setter run during Undo()/Redo() can echo back through whatever observes
    // engine events; that replay is not a new edit.
    if (m_applying) return;

    // Any fresh edit invalidates the redo branch.
    m_redo.clear();

    if (TryMergeIntoTop(command.get())) {
        m_lastPush = Clock::now();
        Notify(STACK_CHANGED_EVENT);
        return;
    }

    command->SetGestureSerial(m_gestureSerial);
    m_undo.push_back(std::move(command));

    // Cap from the front — the oldest history is what we can afford to lose.
    while (m_undo.size() > kHistoryLimit) m_undo.pop_front();

    m_lastPush = Clock::now();
    Notify(STACK_CHANGED_EVENT);
}

void UndoSystem::Undo()
{
    if (m_undo.empty() || m_applying) return;

    std::unique_ptr<Command> command = std::move(m_undo.back());
    m_undo.pop_back();

    m_applying = true;
    command->Undo(container);
    m_applying = false;

    m_redo.push_back(std::move(command));
    BreakMergeChain();   // a later edit must not fold into a command we just moved
    Notify(STACK_CHANGED_EVENT);
}

void UndoSystem::Redo()
{
    if (m_redo.empty() || m_applying) return;

    std::unique_ptr<Command> command = std::move(m_redo.back());
    m_redo.pop_back();

    m_applying = true;
    command->Redo(container);
    m_applying = false;

    m_undo.push_back(std::move(command));
    BreakMergeChain();
    Notify(STACK_CHANGED_EVENT);
}

void UndoSystem::Clear()
{
    m_undo.clear();
    m_redo.clear();
    BreakMergeChain();
    Notify(STACK_CHANGED_EVENT);
}

std::string UndoSystem::UndoText() const
{
    return m_undo.empty() ? std::string() : m_undo.back()->GetText();
}

std::string UndoSystem::RedoText() const
{
    return m_redo.empty() ? std::string() : m_redo.back()->GetText();
}

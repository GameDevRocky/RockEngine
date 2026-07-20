#include "engine/commands/MacroCommand.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/SelectionManager.hpp"

void MacroCommand::Add(std::unique_ptr<Command> child)
{
    if (child) m_children.push_back(std::move(child));
}

std::unique_ptr<Command> MacroCommand::Wrap(std::vector<std::unique_ptr<Command>> children,
                                            std::string text)
{
    // Drop nulls first — call sites collect from factories that return nullptr on
    // failure, and it is cleaner to filter here than at every one of them.
    std::vector<std::unique_ptr<Command>> kept;
    kept.reserve(children.size());
    for (auto& child : children)
        if (child) kept.push_back(std::move(child));

    if (kept.empty()) return nullptr;
    if (kept.size() == 1) return std::move(kept.front());

    auto macro = std::make_unique<MacroCommand>(std::move(text));
    macro->m_children = std::move(kept);
    return macro;
}

void MacroCommand::SetSelectionAfterUndo(std::vector<std::string> ids)
{
    m_selectionAfterUndo = std::move(ids);
}

void MacroCommand::SetSelectionAfterRedo(std::vector<std::string> ids)
{
    m_selectionAfterRedo = std::move(ids);
}

void MacroCommand::ApplySelection(Container* container,
                                  const std::optional<std::vector<std::string>>& ids)
{
    if (!ids.has_value() || !container) return;
    if (auto* selection = container->FindSystem<SelectionManager>())
        selection->SelectMany(*ids);
}

void MacroCommand::Undo(Container* container)
{
    // Reverse — see the class comment.
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
        (*it)->Undo(container);

    // After the children, so it overrides whatever they each selected on the way.
    ApplySelection(container, m_selectionAfterUndo);
}

void MacroCommand::Redo(Container* container)
{
    for (auto& child : m_children)
        child->Redo(container);

    ApplySelection(container, m_selectionAfterRedo);
}

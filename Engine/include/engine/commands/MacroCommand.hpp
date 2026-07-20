#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "engine/core/Command.hpp"

// N commands that undo and redo as ONE history entry.
//
// Undo runs children in REVERSE, Redo forward. Reverse is not cosmetic: commands
// like SubtreeCommand capture a sibling *index* into the live list, and each
// successive destroy shrinks that list. Deleting siblings A(0) and B(1) captures
// A@0, then B@0 (B having shifted down); restoring in reverse yields [B] -> [A,B],
// while forward would yield [A] -> [B,A].
class MacroCommand : public Command {
public:
    explicit MacroCommand(std::string text) : m_text(std::move(text)) {}

    void Add(std::unique_ptr<Command> child);

    // The call-site constructor. Returns nullptr for an empty list and the BARE
    // CHILD for a list of one, so a single-object gesture behaves exactly as it did
    // before macros existed — wrapping a lone PropertyCommand would give it
    // MergeId()==kNoMerge and stop it coalescing with a following inspector edit.
    static std::unique_ptr<Command> Wrap(std::vector<std::unique_ptr<Command>> children,
                                         std::string text);

    // Optional selection to restore after Undo / after Redo, applied AFTER the
    // children run. Unset by default, which leaves the selection alone — the right
    // behaviour for a gizmo macro. Set by the call site, the only place that knows
    // what the gesture's result should be. Ids only, per the Command contract.
    void SetSelectionAfterUndo(std::vector<std::string> ids);
    void SetSelectionAfterRedo(std::vector<std::string> ids);

    void Undo(Container* container) override;
    void Redo(Container* container) override;
    std::string GetText() const override { return m_text; }

    // MergeId stays kNoMerge, and that is forced rather than stylistic:
    // UndoSystem::TryMergeIntoTop only checks that ids match before calling
    // MergeWith, so a mergeable macro would be offered following PropertyCommands
    // and would have to fold a single-property edit into a group whose "before"
    // state it cannot express.

private:
    void ApplySelection(Container* container,
                        const std::optional<std::vector<std::string>>& ids);

    std::vector<std::unique_ptr<Command>> m_children;
    std::string m_text;
    std::optional<std::vector<std::string>> m_selectionAfterUndo;
    std::optional<std::vector<std::string>> m_selectionAfterRedo;
};

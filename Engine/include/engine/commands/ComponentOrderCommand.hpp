#pragma once

#include <memory>
#include <string>

#include "engine/core/Command.hpp"

class GameObject;

// Reorders one component inside its owning GameObject. The command stores ids,
// never pointers, so it remains valid across Inspector rebuilds and resolves
// against the UndoSystem's own container during replay.
class ComponentOrderCommand : public Command {
public:
    static std::unique_ptr<ComponentOrderCommand> Capture(
        GameObject* owner, const std::string& componentId);

    // Captures the resulting index. False means the component did not move.
    bool Commit(GameObject* owner);

    void Undo(Container* container) override;
    void Redo(Container* container) override;
    std::string GetText() const override { return m_text; }

private:
    ComponentOrderCommand() = default;
    void Apply(Container* container, int index);

    std::string m_ownerId;
    std::string m_componentId;
    std::string m_text;
    int m_beforeIndex = -1;
    int m_afterIndex = -1;
};

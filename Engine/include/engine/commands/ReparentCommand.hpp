#pragma once
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include "engine/core/Command.hpp"

class GameObject;

// Moves an object to a different parent and/or sibling position.
//
// Reparent and reorder are the same edit: Scene::ReorderObject performs both
// (it reparents first, since SetParent always appends, then indexes). So undoing
// a reparent means restoring the old parent *and* the old sibling index — the
// parent alone would put the object back at the end of its siblings.
//
// Used as capture-then-commit so it can wrap the existing drop handling without
// changing it:
//
//     auto cmd = ReparentCommand::Capture(object);
//     ... perform the move however the caller likes ...
//     if (cmd && cmd->Commit(object)) undoSystem->Push(std::move(cmd));
class ReparentCommand : public Command {
public:
    // Records where `target` currently sits. Call before moving it.
    static std::unique_ptr<ReparentCommand> Capture(GameObject* target);

    // Records where `target` ended up. Returns false if nothing actually moved,
    // so a drop that resolved to the same place produces no history entry.
    bool Commit(GameObject* target);

    void Undo(Container* container) override;
    void Redo(Container* container) override;
    std::string GetText() const override { return m_text; }

private:
    ReparentCommand() = default;

    // A parent id of "" means the scene root list.
    struct Placement {
        std::string parentId;
        int siblingIndex = 0;
        // SetParent(keepWorld=true) recomputes the local TRS by decomposing
        // inverse(parentWorld) * oldWorld. That round trip is only *mathematically*
        // an identity, so replaying it repeatedly walks the values. Restoring the
        // captured locals instead keeps undo/redo cycles exact.
        glm::vec2 localPosition{0};
        float localRotation = 0.0f;
        glm::vec2 localScale{1};
    };

    static bool ReadPlacement(GameObject* target, Placement& out);
    void ApplyPlacement(Container* container, const Placement& placement);

    std::string m_sceneId;
    std::string m_objectId;
    Placement m_before;
    Placement m_after;
    std::string m_text;
};

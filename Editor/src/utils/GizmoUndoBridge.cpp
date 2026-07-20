#include "utils/GizmoUndoBridge.hpp"
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include "Engine.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/UndoSystem.hpp"
#include "engine/commands/MacroCommand.hpp"
#include "engine/commands/PropertyCommand.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/BoxCollider.hpp"
#include "engine/components/CircleCollider.hpp"
#include "engine/components/CapsuleCollider.hpp"
#include "engine/components/Camera.hpp"
#include "engine/rendering/core/GizmosManager.hpp"

// Translates a committed gizmo gesture into undo commands.
//
// GizmosManager lives in Engine because the gizmos are drawn there, but it only
// *reports* a finished drag — the decision to record one is made here, on the
// editor side, like every other undo call site. Nothing in Engine pushes commands.
namespace {

// Each drag writes a property that already has a setter; reuse those rather than
// inventing gizmo-specific commands, so a gizmo move and an inspector edit produce
// the same kind of entry.
//
// Returns the command instead of pushing it: one gesture can produce many edits
// (position + rotation + scale, and one set per object in a multi-selection), and
// they must land as a single history entry.
std::unique_ptr<Command> MakeGizmoCommand(const GizmoEdit& edit) {
    const std::string& p = edit.property;

    if (p == "localPosition") {
        return std::make_unique<PropertyCommand<Transform, glm::vec2>>(
            edit.targetId, p, std::any_cast<glm::vec2>(edit.before),
            std::any_cast<glm::vec2>(edit.after),
            [](Transform* t, const glm::vec2& v){ t->SetPosition(v); }, "Move");
    }
    if (p == "localRotation") {
        return std::make_unique<PropertyCommand<Transform, float>>(
            edit.targetId, p, std::any_cast<float>(edit.before),
            std::any_cast<float>(edit.after),
            [](Transform* t, const float& v){ t->SetRotation(v); }, "Rotate");
    }
    if (p == "localScale") {
        return std::make_unique<PropertyCommand<Transform, glm::vec2>>(
            edit.targetId, p, std::any_cast<glm::vec2>(edit.before),
            std::any_cast<glm::vec2>(edit.after),
            [](Transform* t, const glm::vec2& v){ t->SetScale(v); }, "Scale");
    }
    if (p == "center") {
        return std::make_unique<PropertyCommand<Collider, glm::vec2>>(
            edit.targetId, p, std::any_cast<glm::vec2>(edit.before),
            std::any_cast<glm::vec2>(edit.after),
            [](Collider* c, const glm::vec2& v){ c->SetCenter(v); }, "Move Collider");
    }
    if (p == "size") {
        return std::make_unique<PropertyCommand<BoxCollider, glm::vec2>>(
            edit.targetId, p, std::any_cast<glm::vec2>(edit.before),
            std::any_cast<glm::vec2>(edit.after),
            [](BoxCollider* c, const glm::vec2& v){ c->SetSize(v); }, "Resize Collider");
    }
    if (p == "radius") {
        // Circle and capsule both expose SetRadius but share no base declaring it,
        // so resolve against whichever type actually matches.
        Container* active = Engine::Get()->GetActiveContainer();
        auto* registry = active ? active->FindSystem<Registry>() : nullptr;
        if (!registry) return nullptr;
        const float before = std::any_cast<float>(edit.before);
        const float after  = std::any_cast<float>(edit.after);

        if (registry->Find<CircleCollider>(edit.targetId)) {
            return std::make_unique<PropertyCommand<CircleCollider, float>>(
                edit.targetId, p, before, after,
                [](CircleCollider* c, const float& v){ c->SetRadius(v); }, "Resize Collider");
        }
        if (registry->Find<CapsuleCollider>(edit.targetId)) {
            return std::make_unique<PropertyCommand<CapsuleCollider, float>>(
                edit.targetId, p, before, after,
                [](CapsuleCollider* c, const float& v){ c->SetRadius(v); }, "Resize Collider");
        }
        return nullptr;
    }
    if (p == "height") {
        return std::make_unique<PropertyCommand<CapsuleCollider, float>>(
            edit.targetId, p, std::any_cast<float>(edit.before),
            std::any_cast<float>(edit.after),
            [](CapsuleCollider* c, const float& v){ c->SetHeight(v); }, "Resize Collider");
    }
    if (p == "orthoSize") {
        return std::make_unique<PropertyCommand<Camera, float>>(
            edit.targetId, p, std::any_cast<float>(edit.before),
            std::any_cast<float>(edit.after),
            [](Camera* c, const float& v){ c->SetOrthoSize(v); }, "Change Camera Size");
    }
    return nullptr;
}

// Name the gesture for the Edit menu: a shared verb when every edit touched the same
// property, "Transform" when a single object moved in several ways at once, and an
// object count when more than one object was involved.
std::string DescribeGesture(const std::vector<GizmoEdit>& edits) {
    if (edits.empty()) return "Transform";

    bool sameProperty = true;
    std::unordered_set<std::string> targets;
    for (const auto& edit : edits) {
        if (edit.property != edits.front().property) sameProperty = false;
        targets.insert(edit.targetId);
    }

    std::string verb = "Transform";
    if (sameProperty) {
        const std::string& p = edits.front().property;
        if      (p == "localPosition") verb = "Move";
        else if (p == "localRotation") verb = "Rotate";
        else if (p == "localScale")    verb = "Scale";
        else if (p == "orthoSize")     verb = "Change Camera Size";
        else                           verb = "Resize Collider";
    }

    if (targets.size() > 1) verb += " " + std::to_string(targets.size()) + " Objects";
    return verb;
}

} // namespace

void GizmoUndoBridge::Install() {
    // GizmosManager is a plain singleton, so it survives the container swap and only
    // needs subscribing once.
    GizmosManager::Get()->Subscribe([](const std::any& data) {
        Container* active = Engine::Get()->GetActiveContainer();
        auto* undoSystem = active ? active->FindSystem<UndoSystem>() : nullptr;
        if (!undoSystem) return true;

        const auto& edits = std::any_cast<const std::vector<GizmoEdit>&>(data);

        // One gesture, one history entry. A universal-gizmo drag reports position,
        // rotation and scale separately, and a multi-object drag reports a set per
        // object — all of it has to undo in a single Ctrl+Z.
        //
        // Grouping is done here and not via the gesture serial: that serial is only
        // read by UndoSystem::TryMergeIntoTop at push time, and Undo() pops exactly
        // one command, so it never grouped anything.
        std::vector<std::unique_ptr<Command>> commands;
        commands.reserve(edits.size());
        for (const auto& edit : edits)
            if (auto command = MakeGizmoCommand(edit)) commands.push_back(std::move(command));

        // Wrap collapses to the bare command when there is only one, so a simple
        // single-axis drag stays exactly what it was before macros existed.
        undoSystem->Push(MacroCommand::Wrap(std::move(commands), DescribeGesture(edits)));
        return true;
    }, GizmosManager::EDIT_COMMITTED_EVENT);
}

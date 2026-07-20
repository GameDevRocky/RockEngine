#include "engine/commands/ReparentCommand.hpp"
#include <algorithm>
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"

bool ReparentCommand::ReadPlacement(GameObject* target, Placement& out)
{
    if (!target) return false;
    Transform* transform = target->GetTransform();
    if (!transform) return false;
    Scene* scene = target->GetScene();
    if (!scene) return false;

    out.localPosition = transform->localPosition;
    out.localRotation = transform->localRotation;
    out.localScale = transform->localScale;

    Transform* parent = transform->GetParent();
    if (parent && parent->GetGameObject()) {
        out.parentId = parent->GetGameObject()->GetID();
        const auto& siblings = parent->GetChildren();
        auto it = std::find(siblings.begin(), siblings.end(), transform);
        out.siblingIndex = static_cast<int>(std::distance(siblings.begin(), it));
    } else {
        out.parentId.clear();
        std::vector<GameObject*> roots = scene->GetRootObjects();
        auto it = std::find(roots.begin(), roots.end(), target);
        out.siblingIndex = static_cast<int>(std::distance(roots.begin(), it));
    }
    return true;
}

std::unique_ptr<ReparentCommand> ReparentCommand::Capture(GameObject* target)
{
    if (!target || !target->GetScene()) return nullptr;

    std::unique_ptr<ReparentCommand> command(new ReparentCommand());
    if (!ReadPlacement(target, command->m_before)) return nullptr;

    command->m_sceneId = target->GetScene()->GetID();
    command->m_objectId = target->GetID();
    command->m_text = "Move " + target->GetName();
    return command;
}

bool ReparentCommand::Commit(GameObject* target)
{
    if (!target || target->GetID() != m_objectId) return false;
    if (!ReadPlacement(target, m_after)) return false;

    // Nothing moved — a drop that resolved back to the same slot.
    return !(m_after.parentId == m_before.parentId &&
             m_after.siblingIndex == m_before.siblingIndex);
}

void ReparentCommand::ApplyPlacement(Container* container, const Placement& placement)
{
    if (!container) return;
    Registry* registry = container->FindSystem<Registry>();
    if (!registry) return;

    Scene* scene = registry->Find<Scene>(m_sceneId);
    GameObject* target = registry->Find<GameObject>(m_objectId);
    if (!scene || !target) return;

    // ReorderObject is the single primitive that restores both halves: it
    // reparents (SetParent appends) and then places the object at the index.
    scene->ReorderObject(m_objectId, placement.parentId, placement.siblingIndex);

    // Undo the drift SetParent(keepWorld=true) introduces by writing back the
    // exact locals recorded for this placement.
    if (Transform* transform = target->GetTransform()) {
        transform->SetPosition(placement.localPosition);
        transform->SetRotation(placement.localRotation);
        transform->SetScale(placement.localScale);
    }
}

void ReparentCommand::Undo(Container* container) { ApplyPlacement(container, m_before); }
void ReparentCommand::Redo(Container* container) { ApplyPlacement(container, m_after); }

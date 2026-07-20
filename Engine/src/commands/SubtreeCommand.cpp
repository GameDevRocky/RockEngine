#include "engine/commands/SubtreeCommand.hpp"
#include <algorithm>
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/core/Scene.hpp"
#include "engine/core/SelectionManager.hpp"
#include "engine/components/Transform.hpp"
#include "engine/serialization/Registry.hpp"

namespace {

// Re-select the subtree root so the result of an undo/redo is visible instead of
// happening off screen. Scoped to the command's own container.
void SelectIn(Container* container, const std::string& id) {
    if (!container) return;
    if (auto* selection = container->FindSystem<SelectionManager>())
        selection->Select(id);
}

} // namespace

void SubtreeCommand::CapturePlacement(GameObject* target)
{
    Scene* scene = target->GetScene();
    if (!scene) return;
    m_sceneId = scene->GetID();

    Transform* transform = target->GetTransform();
    Transform* parent = transform ? transform->GetParent() : nullptr;

    if (parent && parent->GetGameObject()) {
        m_parentId = parent->GetGameObject()->GetID();
        const auto& siblings = parent->GetChildren();
        auto it = std::find(siblings.begin(), siblings.end(), transform);
        m_siblingIndex = static_cast<int>(std::distance(siblings.begin(), it));
    } else {
        m_parentId.clear();
        std::vector<GameObject*> roots = scene->GetRootObjects();
        auto it = std::find(roots.begin(), roots.end(), target);
        m_siblingIndex = static_cast<int>(std::distance(roots.begin(), it));
    }
}

std::unique_ptr<SubtreeCommand> SubtreeCommand::DestroyAndRecord(GameObject* target)
{
    if (!target) return nullptr;
    Scene* scene = target->GetScene();
    if (!scene) return nullptr;

    std::unique_ptr<SubtreeCommand> command(new SubtreeCommand());
    command->m_direction = Direction::Destroy;
    command->m_rootId = target->GetID();
    command->m_text = "Delete " + target->GetName();
    command->CapturePlacement(target);
    command->m_snapshot = scene->SnapshotSubtree(target);

    // Everything is captured; now it is safe to tear the object down.
    target->Shutdown();
    return command;
}

std::unique_ptr<SubtreeCommand> SubtreeCommand::RecordCreated(GameObject* created,
                                                              YAML::Node snapshot,
                                                              std::string text)
{
    if (!created || !created->GetScene()) return nullptr;

    std::unique_ptr<SubtreeCommand> command(new SubtreeCommand());
    command->m_direction = Direction::Create;
    command->m_rootId = created->GetID();
    command->m_snapshot = snapshot;
    command->m_text = std::move(text);
    command->CapturePlacement(created);
    return command;
}

void SubtreeCommand::DoDestroy(Container* container)
{
    if (!container) return;
    Registry* registry = container->FindSystem<Registry>();
    if (!registry) return;

    GameObject* target = registry->Find<GameObject>(m_rootId);
    if (!target) return;
    Scene* scene = target->GetScene();
    if (!scene) return;

    // Re-snapshot rather than reusing the stored copy: the subtree may have been
    // edited since (undo -> edit -> redo), and a later restore has to bring back
    // the current state, not the original.
    m_snapshot = scene->SnapshotSubtree(target);
    CapturePlacement(target);

    // Shutdown() unregisters synchronously, so `target` stops resolving right away
    // and is freed at the next registry flush. Nothing may be read from it below.
    target->Shutdown();
}

void SubtreeCommand::DoCreate(Container* container)
{
    if (!container) return;
    Registry* registry = container->FindSystem<Registry>();
    if (!registry) return;

    Scene* scene = registry->Find<Scene>(m_sceneId);
    if (!scene) return;
    if (registry->Find<GameObject>(m_rootId)) return;   // already live

    if (GameObject* root = scene->RestoreSubtree(m_snapshot, m_parentId, m_siblingIndex))
        SelectIn(container, root->GetID());
}

void SubtreeCommand::Undo(Container* container)
{
    if (m_direction == Direction::Create) DoDestroy(container);
    else                                  DoCreate(container);
}

void SubtreeCommand::Redo(Container* container)
{
    if (m_direction == Direction::Create) DoCreate(container);
    else                                  DoDestroy(container);
}

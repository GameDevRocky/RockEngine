#include "engine/commands/ComponentOrderCommand.hpp"

#include "engine/components/Component.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/serialization/Registry.hpp"

std::unique_ptr<ComponentOrderCommand> ComponentOrderCommand::Capture(
        GameObject* owner, const std::string& componentId) {
    if (!owner) return nullptr;

    const int index = owner->GetComponentIndex(componentId);
    if (index < 0) return nullptr;

    Component* component = nullptr;
    if (Container* container = owner->GetContainer()) {
        if (Registry* registry = container->FindSystem<Registry>())
            component = registry->Find<Component>(componentId);
    }
    if (!component) return nullptr;

    std::unique_ptr<ComponentOrderCommand> command(new ComponentOrderCommand());
    command->m_ownerId = owner->GetID();
    command->m_componentId = componentId;
    command->m_beforeIndex = index;
    command->m_text = "Reorder " + component->GetTypeName();
    return command;
}

bool ComponentOrderCommand::Commit(GameObject* owner) {
    if (!owner || owner->GetID() != m_ownerId) return false;
    m_afterIndex = owner->GetComponentIndex(m_componentId);
    return m_afterIndex >= 0 && m_afterIndex != m_beforeIndex;
}

void ComponentOrderCommand::Apply(Container* container, int index) {
    if (!container || index < 0) return;
    Registry* registry = container->FindSystem<Registry>();
    GameObject* owner = registry ? registry->Find<GameObject>(m_ownerId) : nullptr;
    if (owner) owner->MoveComponent(m_componentId, static_cast<std::size_t>(index));
}

void ComponentOrderCommand::Undo(Container* container) { Apply(container, m_beforeIndex); }
void ComponentOrderCommand::Redo(Container* container) { Apply(container, m_afterIndex); }

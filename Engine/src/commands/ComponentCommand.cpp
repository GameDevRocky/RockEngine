#include "engine/commands/ComponentCommand.hpp"
#include "engine/core/Container.hpp"
#include "engine/core/GameObject.hpp"
#include "engine/components/Component.hpp"
#include "engine/serialization/Registry.hpp"
#include "engine/serialization/SerializableFactory.hpp"

std::unique_ptr<ComponentCommand> ComponentCommand::RemoveAndRecord(GameObject* owner,
                                                                     Component* component)
{
    if (!owner || !component) return nullptr;

    std::unique_ptr<ComponentCommand> command(new ComponentCommand());
    command->m_direction = Direction::Remove;
    command->m_ownerId = owner->GetID();
    command->m_componentId = component->GetID();
    command->m_typeName = component->GetTypeName();
    command->m_snapshot = component->Serialize();
    command->m_text = "Remove " + command->m_typeName;

    owner->RemoveComponent(component);
    return command;
}

std::unique_ptr<ComponentCommand> ComponentCommand::RecordAdded(GameObject* owner,
                                                                 Component* component)
{
    if (!owner || !component) return nullptr;

    std::unique_ptr<ComponentCommand> command(new ComponentCommand());
    command->m_direction = Direction::Add;
    command->m_ownerId = owner->GetID();
    command->m_componentId = component->GetID();
    command->m_typeName = component->GetTypeName();
    command->m_snapshot = component->Serialize();
    command->m_text = "Add " + command->m_typeName;
    return command;
}

void ComponentCommand::DoAdd(Container* container)
{
    if (!container) return;
    Registry* registry = container->FindSystem<Registry>();
    if (!registry) return;

    GameObject* owner = registry->Find<GameObject>(m_ownerId);
    if (!owner) return;
    if (registry->Find<Component>(m_componentId)) return;   // already present

    auto* created = SerializableFactory::Create(m_typeName);
    auto* component = dynamic_cast<Component*>(created);
    if (!component) { delete created; return; }

    // Deserialize before AddComponent: it restores the original id, and
    // AddComponent registers the component under whatever id it carries.
    component->Attach(container);
    component->Deserialize(m_snapshot);

    // AddComponent handles Attach/Register/SetGameObject/Init/PostInit and fires
    // ADD_COMPONENT_EVENT, which is what rebuilds the inspector.
    owner->AddComponent(component);
}

void ComponentCommand::DoRemove(Container* container)
{
    if (!container) return;
    Registry* registry = container->FindSystem<Registry>();
    if (!registry) return;

    GameObject* owner = registry->Find<GameObject>(m_ownerId);
    Component* component = registry->Find<Component>(m_componentId);
    if (!owner || !component) return;

    // Re-snapshot: the component may have been edited since it was recorded
    // (undo -> tweak -> redo), and a later restore has to bring back the current
    // values, not the ones captured when the command was created.
    m_snapshot = component->Serialize();

    owner->RemoveComponent(component);
}

void ComponentCommand::Undo(Container* container)
{
    if (m_direction == Direction::Add) DoRemove(container);
    else                               DoAdd(container);
}

void ComponentCommand::Redo(Container* container)
{
    if (m_direction == Direction::Add) DoAdd(container);
    else                               DoRemove(container);
}

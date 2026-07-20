#pragma once
#include <memory>
#include <string>
#include "yaml-cpp/yaml.h"
#include "engine/core/Command.hpp"

class GameObject;
class Component;

// Add or remove a single component, inverted.
//
// Both directions carry a YAML snapshot so the component comes back with its
// ORIGINAL id — every reference to it (GameObject::component_ids, ScriptComponent
// object-ref fields) is an id string, so a fresh UUID would silently orphan them.
//
// Nothing here is wired into GameObject::AddComponent/RemoveComponent. Those stay
// untouched so engine- and script-driven changes never enter the history; only
// editor call sites construct these.
class ComponentCommand : public Command {
public:
    enum class Direction { Add, Remove };

    // Removes `component` from `owner` and returns the command that restores it.
    // Snapshots before the removal — afterwards the component is already shut down
    // and there is nothing left to record, so doing the removal here keeps that
    // ordering impossible to get wrong at a call site.
    static std::unique_ptr<ComponentCommand> RemoveAndRecord(GameObject* owner,
                                                              Component* component);

    // Records a component the caller has already attached via AddComponent.
    static std::unique_ptr<ComponentCommand> RecordAdded(GameObject* owner,
                                                          Component* component);

    void Undo(Container* container) override;
    void Redo(Container* container) override;
    std::string GetText() const override { return m_text; }

private:
    ComponentCommand() = default;

    void DoAdd(Container* container);
    void DoRemove(Container* container);

    Direction m_direction = Direction::Add;
    std::string m_ownerId;
    std::string m_componentId;
    std::string m_typeName;
    YAML::Node m_snapshot;
    std::string m_text;
};

#pragma once
#include <memory>
#include <string>
#include "yaml-cpp/yaml.h"
#include "engine/core/Command.hpp"

class GameObject;

// Create or destroy a whole GameObject subtree, backed by the
// Scene::SnapshotSubtree / Scene::RestoreSubtree YAML round-trip.
//
// Creating and destroying are the same operation inverted, so one class with a
// direction flag covers New GameObject, Delete and Duplicate.
//
// The snapshot preserves ids verbatim, which is what makes this work at all: a
// restored object is the *same* object as far as every id-based reference in the
// engine is concerned, so script fields and selection still resolve to it.
class SubtreeCommand : public Command {
public:
    enum class Direction { Create, Destroy };

    // Destroys `target` and returns the command that can bring it back. The
    // snapshot is taken BEFORE the shutdown — afterwards the object is already
    // unregistered and there would be nothing left to record, so doing the destroy
    // here is what keeps that ordering impossible to get wrong at a call site.
    // Returns nullptr (and destroys nothing) if `target` has no scene.
    static std::unique_ptr<SubtreeCommand> DestroyAndRecord(GameObject* target);

    // Records an object the caller has already created. `snapshot` must be
    // Scene::SnapshotSubtree output for it — for a duplicate, the post-remap YAML
    // DuplicateGameObject hands back, so redo reproduces identical ids instead of
    // minting fresh ones.
    static std::unique_ptr<SubtreeCommand> RecordCreated(GameObject* created,
                                                         YAML::Node snapshot,
                                                         std::string text);

    void Undo(Container* container) override;
    void Redo(Container* container) override;
    std::string GetText() const override { return m_text; }

private:
    SubtreeCommand() = default;

    void DoCreate(Container* container);
    void DoDestroy(Container* container);
    // Records parent id ("" = scene root) and sibling index. Must run while the
    // object is still in the hierarchy.
    void CapturePlacement(GameObject* target);

    Direction m_direction = Direction::Create;
    std::string m_sceneId;
    std::string m_rootId;
    std::string m_parentId;
    int m_siblingIndex = 0;
    YAML::Node m_snapshot;
    std::string m_text;
};
